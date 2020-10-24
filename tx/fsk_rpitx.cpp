/*
  fsk_rpitx.cpp
  David Rowe July 2020

  FSK modulates an input bit stream using rpitx.

  TODO:
  [ ] decide if we should have packet bytes input data
  [ ] if so, need a way to send uncoded test packets as per examples in README.md
  [ ] Something wrong at Rs<200, e.g. Rx can't detect any packets at Rs=100
  [ ] test link with other codes
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>

#include "../librpitx/src/librpitx.h"
#include "ldpc_codes.h"
#include "freedv_api.h"

#define MAX_CHAR 256

// Functions to build FSK_LDPC frame.  Not normally exposed by FreeDV API
extern "C" {
int freedv_tx_fsk_ldpc_bits_per_frame(struct freedv *f);
void freedv_tx_fsk_ldpc_framer(struct freedv *f, uint8_t frame[], uint8_t payload_data[]);
void ofdm_generate_payload_data_bits(uint8_t payload_data_bits[], int n);
unsigned short freedv_gen_crc16(unsigned char* data_p, int length);
void freedv_pack(unsigned char *bytes, unsigned char *bits, int nbits);
void freedv_unpack(unsigned char *bits, unsigned char *bytes, int nbits);
}

bool running=true;
static int terminate_calls = 0;

static void terminate(int num) {
    terminate_calls++;
    running=false;
    fprintf(stderr,"Caught signal %d - Terminating\n", num);
    if (terminate_calls >= 5) exit(1);
}


/* Use the Linux /sys/class/gpio system to access the RPis GPIOs */
void sys_gpio(const char filename[], const char s[]) {
    FILE *fgpio = fopen(filename, "wt");
    //fprintf(stderr,"%s %s\n",filename, s);
    if (fgpio == NULL) {
      fprintf(stderr, "\nProblem opening %s\n", filename);
        exit(1);
    }
    fprintf(fgpio,"%s",s);
    fclose(fgpio);
}

// calculate and insert CRC in the last 16 bits of (unpacked) data_bits[]
void calculate_and_insert_crc(uint8_t data_bits[], int data_bits_per_frame) {            
    assert((data_bits_per_frame % 8) == 0);
    int data_bytes_per_frame = data_bits_per_frame / 8;
    uint8_t data_bytes[data_bytes_per_frame];
    freedv_pack(data_bytes, data_bits, data_bits_per_frame-16);
    uint16_t crc16 = freedv_gen_crc16(data_bytes, data_bytes_per_frame-2);
    uint8_t crc16_bytes[] = { (uint8_t)(crc16 >> 8), (uint8_t)(crc16 & 0xff) };
    freedv_unpack(data_bits+data_bits_per_frame-16, crc16_bytes, 16);
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
    int Nbursts = 1;
    char ant_switch_gpio[128] = "";
    char ant_switch_gpio_path[MAX_CHAR] = "";

    char usage[] = "usage: %s [-m fskM 2|4] [-f carrierFreqHz] [-r symbRateHz] [-s shiftHz] [-t] [-c] "
                   "[--testframes Nframes] InputOneBitPerCharFile\n"
                   "  -c               Carrier test mode\n"
                   "  -g               GPIO that controls antenna Tx/Rx switch\n"
                   "  -t               ...0101010... FSK test mode\n"
                   "  --code CodeName  Use LDPC code CodeName\n"
                   "  --listcodes      List available LDPC codes\n"
                   "  --testframes N   Send N testframes per burst\n"
                   "  --bursts     B   Send B bursts of N testframes (default 1)\n"
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
            {"bursts",    required_argument, 0, 'e'},
            {0, 0, 0, 0}
        };
        
        opt = getopt_long(argc,argv,"a:bce:f:g:m:r:s:tu:",long_opts,&opt_idx);
        
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
        case 'e':
            Nbursts = atoi(optarg);
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
        case 'g':
            strcpy(ant_switch_gpio, optarg);
            sys_gpio("/sys/class/gpio/unexport", ant_switch_gpio);
            sys_gpio("/sys/class/gpio/export", ant_switch_gpio);
            usleep(100*1000); /* short delay so OS can create the next device */
            char tmp[MAX_CHAR];
            sprintf(tmp,"/sys/class/gpio/gpio%s/direction", ant_switch_gpio);
            sys_gpio(tmp, "out");
            sprintf(ant_switch_gpio_path,"/sys/class/gpio/gpio%s/value", ant_switch_gpio);
            sys_gpio(ant_switch_gpio_path, "0");
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
        
    int npreamble_symbols = 50*(m>>1);
    int npreamble_bits = npreamble_symbols*(m>>1);
    uint8_t preamble_bits[npreamble_bits];

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

        /* set up preamble */
        /* TODO: this should be a freeDV API function */        
        // cycle through all 2 and 4FSK symbols, not sure if this is better than random
        int sym = 0;
        for(int i=0; i<npreamble_bits; i+=2) {
            preamble_bits[i]   = (sym>>1) & 0x1;
            preamble_bits[i+1] = sym & 0x1;
            sym += 1;
        }

    } else {
        // uncoded mode
        data_bits_per_frame = log2(m);
        bits_per_frame = data_bits_per_frame;
    }
    
    // Set shiftHz at 2*Rs if no command line argument
    if (shiftHz == -1)
        shiftHz = 2*SymbolRate;
    fmmod = new ngfmdmasync(frequency,SymbolRate,14,100); 	
    
    fprintf(stderr, "Frequency: %4.1f MHz Rs: %4.1f kHz Shift: %4.1f kHz M: %d \n", frequency/1E6, SymbolRate/1E3, shiftHz/1E3, m);
    fprintf(stderr, "data_bits_per_frame: %d bits_per_frame: %d\n", data_bits_per_frame, bits_per_frame);
    
    if ((carrier_test == 0) && (one_zero_test == 0)) {
        // FSK Tx --------------------------------------------------------------------

        uint8_t data_bits[data_bits_per_frame];
        uint8_t tx_frame[bits_per_frame];

        if (testframes) {
            /* FSK_LDPC Tx in test frame mode */

            assert(fsk_ldpc);
            ofdm_generate_payload_data_bits(data_bits, data_bits_per_frame);
            calculate_and_insert_crc(data_bits, data_bits_per_frame);            
            freedv_tx_fsk_ldpc_framer(freedv, tx_frame, data_bits);

            // antenna switch to Tx
            if (*ant_switch_gpio_path) sys_gpio(ant_switch_gpio_path, "1");
            
            for(int b=0; b<Nbursts; b++) {

                // transmitter carrier on
                fmmod->clkgpio::enableclk(4);

                // send pre-amble at start of burst
                modulate_frame(fmmod, shiftHz, m, preamble_bits, npreamble_bits);
        
                for (int f=0; f<Nframes; f++) {
                    modulate_frame(fmmod, shiftHz, m, tx_frame, bits_per_frame);

                    // allow early exit on Crtl-C
                    if (!running) goto finished;
                }

                // TODO: try to determine when FIFO is empty instead of arbitrary delay
                float VCOfreqHz = 0;
                for(int i=0; i<50; i++)
                    fmmod->SetFrequencySamples(&VCOfreqHz,1);
                printf("End of this burst\n");
                
                // transmitter carrier off between bursts
                fmmod->clkgpio::disableclk(4);
 
                // Two frames delay so we have some interpacket silence
                float tdelay = (2.0/SymbolRate)*bits_per_frame/(m>>1);
                usleep((int)(tdelay*1E6));
           }

            // antenna switch to Rx
            if (*ant_switch_gpio_path) sys_gpio(ant_switch_gpio_path, "0");
        }
        else {
            /* regular FSK or FSK_LDPC Tx operation with bits/bytes from stdin */     

            if (fsk_ldpc)
                modulate_frame(fmmod, shiftHz, m, preamble_bits, npreamble_bits);
        
            while(running) {
                int BytesRead = fread(data_bits, sizeof(uint8_t), data_bits_per_frame, fin);
                if (BytesRead == (data_bits_per_frame)) {
                    if (fsk_ldpc) {
                        calculate_and_insert_crc(data_bits, data_bits_per_frame);            
                        freedv_tx_fsk_ldpc_framer(freedv, tx_frame, data_bits);
                    }
                    else {
                        memcpy(tx_frame, data_bits, data_bits_per_frame);
                    }
                    modulate_frame(fmmod, shiftHz, m, preamble_bits, npreamble_bits);
                } else {
                    running=false;
                }
            }
        }
    }

    if (carrier_test) {
        fprintf(stderr, "Carrier test mode 1 sec on/off , Ctrl-C to exit\n");
        int count = 0;
        float VCOfreqHz = 0;
        while(running) {
            fmmod->SetFrequencySamples(&VCOfreqHz,1);
            count++;
            if (count == SymbolRate)
                fmmod->clkgpio::disableclk(4);
            if (count == 2*SymbolRate) {
                fmmod->clkgpio::enableclk(4);
                count = 0;
            }
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

    finished:
    
    // this was required to prevent errors on final frame, I suspect
    // as fmmod FIFO hasn't emptied by the time we delete fmmod.
    // Seems a bit wasteful, so there might be a better way

    float VCOfreqHz = 0;
    for(int i=0; i<50; i++)
        fmmod->SetFrequencySamples(&VCOfreqHz,1);
    printf("End of Tx\n");
  
    if (fsk_ldpc) freedv_close(freedv);
    delete fmmod;

    if (*ant_switch_gpio) {
        sys_gpio(ant_switch_gpio_path, "0");
        sys_gpio("/sys/class/gpio/unexport", ant_switch_gpio);
    }
    return 0;
}
	



