/*
  fsk_rpitx.cpp
  David Rowe July 2020

  FSK modulates an input bit stream using rpitx.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <signal.h>
#include <getopt.h>

#include "../librpitx/src/librpitx.h"
#include "ldpc_codes.h"
#include "freedv_api.h"

// not normally exposed by FreeDV API
extern "C" {
int freedv_tx_fsk_ldpc_bits_per_frame(struct freedv *f);
void freedv_tx_fsk_ldpc_framer(struct freedv *f, uint8_t frame[], uint8_t payload_data[]);
void ofdm_generate_payload_data_bits(uint8_t payload_data_bits[], int n);
}

bool running=true;
static int terminate_calls = 0;

static void terminate(int num) {
    terminate_calls++;
    running=false;
    fprintf(stderr,"Caught signal %d - Terminating\n", num);
    if (terminate_calls >= 5) exit(1);
}


void modulate_frame(ngfmdmasync *fmmod, float shiftHz, int m, uint8_t tx_frame[], int bits_per_frame) {
    for(int bit_i=0; bit_i<bits_per_frame;) {
        /* generate the symbol number from the bit stream, 
           e.g. 0,1 for 2FSK, 0,1,2,3 for 4FSK */

        int sym = 0;
        for(int i=m; i>>=1; ) {
            uint8_t bit = tx_frame[bit_i] & 0x1;
            sym = (sym<<1)|bit;
            bit_i++;
        }
        float VCOfreqHz = shiftHz*sym;
        fmmod->SetFrequencySamples(&VCOfreqHz,1);
    }
}

int main(int argc, char **argv)
{
    for (int i = 0; i < 64; i++) {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = terminate;
        sigaction(i, &sa, NULL);
    }

    FILE *fin;
    int   carrier_test = 0;
    int   one_zero_test = 0;
    float frequency = 144.5e6;
    int   SymbolRate = 10000;
    int   m = 2;
    int   log2m;
    float shiftHz = -1;
    ngfmdmasync *fmmod;
    struct freedv *freedv = NULL;
    struct freedv_advanced adv;
    int fsk_ldpc = 0;
    int data_bits_per_frame;           // number of payload data bits
    int bits_per_frame;                // total number of bits including UW, payload data, parity bits for FSK + LDPC mode
    int testframes = 0;
    int Nframes = 0;
    int frames = 0;
    
    char usage[] = "usage: %s [-m fskM 2|4] [-f carrierFreqHz] [-r symbRateHz] [-s shiftHz] [-t] [-c] "
                   "[--testframes Nframes] InputOneBitPerCharFile\n"
                   "  -c               Carrier test mode\n"
                   "  -t               ...0101010... FSK test mode\n"
                   "  --code CodeName  Use LDPC code CodeName\n"
                   "  --listcodes      List available LDPC codes\n"
                   "  --testframes     built in testframes\n"
                   "\n"
                   " Example 1, send 10000 bits of (100 bit) tests frames from external test frame generator\n"
                   " at 1000 bits/s using 2FSK:\n\n"
                   "   $ ../codec2/build_linux/src/fsk_get_test_bits - 10000 | sudo ./fsk_rpitx - -r 1000 -s 1000\n\n"
                   " Example 2, send two LDPC encoded test frames at 1000 bits/s using 2FSK:\n\n"
                   "   $ sudo ./fsk_rpitx /dev/zero --code H_256_512_4 -r 1000 -s 1000 --testframes 2\n";
    
    int opt = 0;
    int opt_idx = 0;
    while( opt != -1 ){
        static struct option long_opts[] = {
            {"code",      required_argument, 0, 'a'},
            {"listcodes", no_argument,       0, 'b'},
            {"testframes",required_argument, 0, 'u'},
            {0, 0, 0, 0}
        };
        
        opt = getopt_long(argc,argv,"a:bm:f:r:s:tcu:",long_opts,&opt_idx);
        
        switch (opt) {
        case 'a':
            fsk_ldpc = 1;
            adv.codename = optarg;
            break;
        case 'b':
            ldpc_codes_list();
            exit(0);
            break;
        case 'c':
            carrier_test = 1;
            break;
        case 'm':
            m = atoi(optarg);
            break;
        case 'f':
            frequency = atof(optarg);
            break;
        case 'r':
            SymbolRate = atof(optarg);
            break;
        case 's':
            shiftHz = atof(optarg);
            break;
        case 't':
            one_zero_test = 1;
            break;
        case 'u':
            if (fsk_ldpc == 0) {
                fprintf(stderr, "internal testframe mode only supported in --code coded mode!\n");
                exit(1);
            }                  
            testframes = 1;
            Nframes = atoi(optarg);
            fprintf(stderr, "Sending %d testframes...\n", Nframes);
            break;
        case 'h':
            fprintf(stderr, usage, argv[0]);
            exit(1);
        }
    }

    if (argc < 2) {
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }

    fin = stdin;
    if (!(carrier_test || one_zero_test)) {
        if (strcmp(argv[optind],"-") != 0) {
            fin = fopen(argv[optind],"rb");
            if (fin == NULL) {
                fprintf(stderr, "Error opening input file: %s\n", argv[optind]);
                exit(1);
            }
        }
    }
        
    if (fsk_ldpc) {
        // setup LDPC encoder and framer
        adv.Rs = SymbolRate;
        adv.Fs = 8*SymbolRate; // just required to satisfy freedv_open FSK_LDPC, as we don't run FSK demod here
        adv.M = m;
        freedv = freedv_open_advanced(FREEDV_MODE_FSK_LDPC, &adv);
        assert(freedv != NULL);
        data_bits_per_frame = freedv_get_bits_per_modem_frame(freedv);
        bits_per_frame = freedv_tx_fsk_ldpc_bits_per_frame(freedv);
        fprintf(stderr, "FSK LDPC mode code: %s data_bits_per_frame: %d\n", adv.codename, data_bits_per_frame);
     } else {
        // uncoded mode
        data_bits_per_frame = log2(m);
        bits_per_frame = data_bits_per_frame;
    }
    
    // Set shiftHz at 2*Rs if no command line argument
    if (shiftHz == -1)
        shiftHz = 2*SymbolRate;
    fmmod = new ngfmdmasync(frequency,SymbolRate,14,100); 	
    padgpio pad;
    pad.setlevel(7); // Set max power

    fprintf(stderr, "Frequency: %4.1f MHz Rs: %4.1f kHz Shift: %4.1f kHz M: %d \n", frequency/1E6, SymbolRate/1E3, shiftHz/1E3, m);

    fprintf(stderr, "data_bits_per_frame: %d bits_per_frame: %d\n", data_bits_per_frame, bits_per_frame);
    
    if ((carrier_test == 0) && (one_zero_test == 0)) { 
        /* regular FSK modulator operation */     

        /* pre-amble */
        int np = 100;
        uint8_t preamble_bits[np];
        for(int i=0; i<np; i++) preamble_bits[i] = rand() % 0x1;
        modulate_frame(fmmod, shiftHz, m, preamble_bits, np);
        
        while(running) {
            uint8_t data_bits[data_bits_per_frame];
            int BytesRead = fread(data_bits, sizeof(uint8_t), data_bits_per_frame, fin);
            if (BytesRead == data_bits_per_frame) {
                if (testframes) {
                    /* replace input data with testframe */
                    ofdm_generate_payload_data_bits(data_bits, data_bits_per_frame);
                }
                uint8_t tx_frame[bits_per_frame];
                if (fsk_ldpc)
                    freedv_tx_fsk_ldpc_framer(freedv, tx_frame, data_bits);
                else
                    memcpy(tx_frame, data_bits, data_bits_per_frame);
                modulate_frame(fmmod, shiftHz, m, tx_frame, bits_per_frame);
            }
            else
                running=false;
            frames++;
            if (testframes)
                if (frames >= Nframes) running = false;
        }

        /* post-amble */
        modulate_frame(fmmod, shiftHz, m, preamble_bits, np);
    }

    if (carrier_test) {
        fprintf(stderr, "Carrier test mode, Ctrl-C to exit\n");
        float VCOfreqHz = 0;
        while(running) {
            fmmod->SetFrequencySamples(&VCOfreqHz,1);
        }
    }

    if (one_zero_test) {
        fprintf(stderr, "...010101... test mode, Ctrl-C to exit\n");
        float VCOfreqHz = 0;
        while(running) {
            if (VCOfreqHz == shiftHz)
                VCOfreqHz = 0;
            else
                VCOfreqHz = shiftHz;
            fmmod->SetFrequencySamples(&VCOfreqHz,1);
        }
    }
  
    printf("End of Tx\n");
    if (fsk_ldpc) freedv_close(freedv);
    delete fmmod;
    return 0;
}
	



