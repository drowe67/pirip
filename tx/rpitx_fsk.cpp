/*
  rpitx_fsk.cpp
  David Rowe July 2020

  FSK modulates an input bit stream using rpitx.

  TODO:
  [X] option for packed/unpacked byte input data
  [ ] Fix operation at Rs<200, at present we can't detect any packets at Rs=100
  [ ] test with other LDPC codes
      + high rate codes are of interest, big gains for small overhead
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
#include <fcntl.h>

#include "../librpitx/src/librpitx.h"
#include "ldpc_codes.h"
#include "freedv_api.h"

#define MAX_CHAR  256
#define FIFO_SIZE 100

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
ngfmdmasync *fmmod = NULL;

static void terminate(int num) {
    terminate_calls++;
    running=false;
    fprintf(stderr,"Caught signal %d - Terminating\n", num);
    if (terminate_calls >= 5) {
        // make sure TX is off if we have to abort
        if (fmmod) delete fmmod;
        exit(1);
    }
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
    assert(fmmod != NULL);
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
    struct freedv *freedv = NULL;
    struct freedv_advanced adv;
    int fsk_ldpc = 0;
    int data_bits_per_frame;           // number of payload data bits
    int bits_per_frame;                // total number of bits including UW, payload data, parity bits for FSK + LDPC mode
    int testframes = 0;
    int Nframes = 0;
    int Nbursts = 1;
    int sequence_numbers = 0;
    char ant_switch_gpio[128] = "";
    char ant_switch_gpio_path[MAX_CHAR] = "";
    int rpitx_fsk_fifo = 0;
    int packed = 0;
    uint8_t source_byte = 0;
    
    char usage[] = "usage: %s [-m fskM 2|4] [-f carrierFreqHz] [-r symbRateHz] [-s shiftHz] [-t] [-c] "
                   "[--testframes Nframes] InputOneBitPerCharFile\n"
                   "  -c               Carrier test mode\n"
                   "  -g               GPIO that controls antenna Tx/Rx switch (1 for Tx, 0 for Rx)\n"
                   "  -t               ...0101010... FSK test mode\n"
                   "  --code CodeName  Use LDPC code CodeName\n"
                   "  --listcodes      List available LDPC codes\n"
                   "  --testframes N   Send N testframes per burst\n"
                   "  --bursts     B   Send B bursts of N testframes (default 1)\n"
                   "  --seq            send packet sequence numbers (breaks testframe BER counting) in byte[1]\n"
                   "  --fifo fifoName  send stats messages to fifoName\n"     
                   "  --packed         packed byte input\n"     
                   "  --source Byte    insert a (non-zero) source address att byte[0]\n"
                   "\n"
                   " Example 1, send 10000 bits of (100 bit) tests frames from external test frame generator\n"
                   " at 1000 bits/s using 2FSK:\n\n"
                   "   $ ../codec2/build_linux/src/fsk_get_test_bits - 10000 | sudo ./rpitx_fsk - -r 1000 -s 1000\n\n"
                   " Example 2, send two LDPC encoded test frames at 1000 bits/s using 2FSK:\n\n"
                   "   $ sudo ./rpitx_fsk /dev/zero --code H_256_512_4 -r 1000 -s 1000 --testframes 2\n";
    
    int opt = 0;
    int opt_idx = 0;
    while( opt != -1 ){
        static struct option long_opts[] = {
            {"code",      required_argument, 0, 'a'},
            {"listcodes", no_argument,       0, 'b'},
            {"testframes",required_argument, 0, 'u'},
            {"bursts",    required_argument, 0, 'e'},
            {"seq",       no_argument,       0, 'q'},
            {"fifo",      required_argument, 0, 'i'},
            {"packed",    no_argument,       0, 'l'},
            {"source",    required_argument, 0, 'd'},
            {0, 0, 0, 0}
        };
        
        opt = getopt_long(argc,argv,"a:bcd:e:f:g:i:m:qr:s:tu:",long_opts,&opt_idx);

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
        case 'd':
            source_byte = strtol(optarg, NULL, 0);
            fprintf(stderr,"source byte: 0x%02x\n", source_byte);
            break;
        case 'e':
            Nbursts = atoi(optarg);
            break;
        case 'm':
            m = atoi(optarg);
            break;
        case 'l':
            packed = 1;
            break;
        case 'f':
            frequency = atof(optarg);
            break;
        case 'i':
            rpitx_fsk_fifo = open(optarg, O_WRONLY);
            if (rpitx_fsk_fifo == -1) {
                fprintf(stderr, "Error opening fifo %s\n", argv[2]);
                exit(1);
            }
            fprintf(stderr, "rpitx_fsk: FIFO opened OK ...\n");
            break;
        case 'q':
            sequence_numbers = 1;
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
            fprintf(stderr, "rpitx_fsk: Sending %d testframe(s)...\n", Nframes);
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
        case '?':
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
        fprintf(stderr, "rpitx_fsk: FSK LDPC mode code: %s data_bits_per_frame: %d\n", adv.codename, data_bits_per_frame);

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
    //fmmod = new ngfmdmasync(frequency,SymbolRate,14,FIFO_SIZE); 	
    //fmmod->clkgpio::disableclk(4);
    
    fprintf(stderr, "rpitx_fsk: Frequency: %4.1f MHz Rs: %4.1f kHz Shift: %4.1f kHz M: %d \n", frequency/1E6, SymbolRate/1E3, shiftHz/1E3, m);
    fprintf(stderr, "rpitx_fsk: data_bits_per_frame: %d bits_per_frame: %d\n", data_bits_per_frame, bits_per_frame);
    
    if ((carrier_test == 0) && (one_zero_test == 0)) {
        // FSK Tx --------------------------------------------------------------------

        uint8_t data_bits[data_bits_per_frame];
        uint8_t tx_frame[bits_per_frame];

        if (testframes) {
            /* FSK_LDPC Tx in test frame mode */

            assert(fsk_ldpc);
            ofdm_generate_payload_data_bits(data_bits, data_bits_per_frame);

            // antenna switch to Tx
            if (*ant_switch_gpio_path) sys_gpio(ant_switch_gpio_path, "1");
            
            for(int b=0; b<Nbursts; b++) {

                // transmitter carrier on
                fmmod = new ngfmdmasync(frequency,SymbolRate,14,FIFO_SIZE);

                // send pre-amble at start of burst
                modulate_frame(fmmod, shiftHz, m, preamble_bits, npreamble_bits);
        
                for (int f=0; f<Nframes; f++) {
                    // optional injection of source address byte
                    if (source_byte) {
                         for (int i=0; i<8; i++) 
                            data_bits[i] = (source_byte >> (7-i)) & 0x1;
                    }
                    
                    // optional injection of sequence numbers to help locate bad frames
                    if (sequence_numbers) {
                        int seq = (f+1) & 0xff;
                        for (int i=0; i<8; i++) 
                            data_bits[8+i] = (seq >> (7-i)) & 0x1;
                    }
                    
                    calculate_and_insert_crc(data_bits, data_bits_per_frame);
                    freedv_tx_fsk_ldpc_framer(freedv, tx_frame, data_bits);
                    modulate_frame(fmmod, shiftHz, m, tx_frame, bits_per_frame);

                    // allow early exit on Crtl-C
                    if (!running) goto finished;
                }

                // wait for enough time for FIFO to empty
                int bufferSamples = FIFO_SIZE - fmmod->GetBufferAvailable();
                float tdelay = (float)bufferSamples/SymbolRate;
                usleep((int)(tdelay*1E6));

                printf("rpitx_fsk: End of this burst\n");
                
                // transmitter carrier off between bursts
                delete fmmod;
 
                // Two frames delay so we have some interpacket silence
                tdelay = (2.0/SymbolRate)*bits_per_frame/(m>>1);
                usleep((int)(tdelay*1E6));
           }

            // antenna switch to Rx
            if (*ant_switch_gpio_path) sys_gpio(ant_switch_gpio_path, "0");
        }
        else {
            /* regular FSK or FSK_LDPC Tx operation with bits/bytes from stdin */     
            int nframes = 0;
            while(1) {
                uint8_t burst_control;
                int nRead;
                
                if (fsk_ldpc) {
                    // in fsk_ldpc mode we prepend input data with a burst control byte
                    nRead = fread(&burst_control, sizeof(uint8_t), 1, fin);
                    if (nRead == 0) goto finished;
                }
                
                if (packed) {
                    int data_bytes_per_frame = data_bits_per_frame/8;
                    uint8_t data_bytes[data_bytes_per_frame];
                    nRead = fread(data_bytes, sizeof(uint8_t), data_bytes_per_frame, fin);
                    freedv_unpack(data_bits, data_bytes, data_bits_per_frame);
                    nRead *= 8;
                }
                else {
                    nRead = fread(data_bits, sizeof(uint8_t), data_bits_per_frame, fin);
                }
                
                fprintf(stderr, "rpitx_fsk: burst_control: %d nRead: %d\n", burst_control, nRead);

                if (nRead != data_bits_per_frame) goto finished;

                if (fsk_ldpc) {

                    // start of burst
                    if (burst_control == 1) {
                        fprintf(stderr, "rpitx_fsk: Tx on\n");
                        // antenna switch to Tx
                        if (*ant_switch_gpio_path) sys_gpio(ant_switch_gpio_path, "1");
                        // transmitter carrier on
                        fmmod = new ngfmdmasync(frequency,SymbolRate,14,FIFO_SIZE);
                        // send preamble
                        fprintf(stderr, "rpitx_fsk: sending preamble\n");
                        modulate_frame(fmmod, shiftHz, m, preamble_bits, npreamble_bits);
                        nframes = 0;
                    }

                    if ((burst_control == 0) || (burst_control == 1)) {
                        fprintf(stderr, "rpitx_fsk: sending frame: %d\n", nframes); nframes++;
                        // send a data frame, note last two bytes in frame replaced with CRC
                        calculate_and_insert_crc(data_bits, data_bits_per_frame);            
                        freedv_tx_fsk_ldpc_framer(freedv, tx_frame, data_bits);
                        modulate_frame(fmmod, shiftHz, m, tx_frame, bits_per_frame);
                    }
                    
                    // end of burst - this has a dummy data frame so don't send
                    if (burst_control == 2) {
                        // wait for enough time for FIFO to empty
                        int bufferSamples = FIFO_SIZE - fmmod->GetBufferAvailable();
                        float tdelay = (float)bufferSamples/SymbolRate;
                        usleep((int)(tdelay*1E6));
                        // transmitter carrier off between bursts
                        //fmmod->clkgpio::disableclk(4);
                        delete fmmod;
                        // antenna switch to Rx
                        if (*ant_switch_gpio_path) sys_gpio(ant_switch_gpio_path, "0");
                        fprintf(stderr, "rpitx_fsk: Tx off\n");
                        if (rpitx_fsk_fifo) {
                            char buf[256];
                            sprintf(buf, "Tx off");
                            if (write(rpitx_fsk_fifo, buf, strlen(buf)+1) ==-1) {
                                fprintf(stderr, "rpitx_fsk: error writing to FIFO\n");
                            }
                        }
                    }
                }
                else {
                    // uncoded mode - just send data_bits without any further framing
                    memcpy(tx_frame, data_bits, data_bits_per_frame);
                    modulate_frame(fmmod, shiftHz, m, tx_frame, bits_per_frame);
                }

                // allow us to bail on Ctrl-C
                if (!running) goto finished;
            }
        }
    }

    if (carrier_test) {
        fprintf(stderr, "Carrier test mode 1 sec on/off , Ctrl-C to exit\n");
        int count = 0;
        float VCOfreqHz = 0;
        fmmod = new ngfmdmasync(frequency,SymbolRate,14,FIFO_SIZE);
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
        delete fmmod;
    }

    if (one_zero_test) {
        fprintf(stderr, "...010101... test mode, Ctrl-C to exit\n");
        float VCOfreqHz = 0;
        fmmod = new ngfmdmasync(frequency,SymbolRate,14,FIFO_SIZE);
        while(running) {
            if (VCOfreqHz == shiftHz)
                VCOfreqHz = 0;
            else
                VCOfreqHz = shiftHz;
            fmmod->SetFrequencySamples(&VCOfreqHz,1);
        }
        delete fmmod;
    }

    finished:
    
    if (fsk_ldpc) freedv_close(freedv);

    if (*ant_switch_gpio) {
        sys_gpio(ant_switch_gpio_path, "0");
        sys_gpio("/sys/class/gpio/unexport", ant_switch_gpio);
    }
    return 0;
}
	



