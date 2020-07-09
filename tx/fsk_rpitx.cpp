/*
  fsk_rpitx.cpp
  David Rowe July 2020

  FSK modulates a one bit per char input bit stream using rpitx.

  TODO:
  [X] works with previous tx-rx test
  [ ] no input file needed for test mode
  [ ] create our own repo/clones librpitx/cmake/README.md
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <signal.h>
#include <getopt.h>

#include "../librpitx/src/librpitx.h"

bool running=true;

static void terminate(int num) {
  running=false;
  fprintf(stderr,"Caught signal - Terminating\n");
   
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
    int   one_zero_test = 0;
    float frequency = 144.5e6;
    int   SymbolRate = 10000;
    int   m = 2;
    int   log2m;
    float shiftHz = -1;
    ngfmdmasync *fmmod;
    
    char usage[] = "usage: %s [-m fskM 2|4] [-f carrierFreqHz] [-r symbRateHz] [-s shiftHz] [-t] InputOneBitPerCharFile\n";

    int opt;
    while ((opt = getopt(argc, argv, "m:f:r:t")) != -1) {
        switch (opt) {
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
        default:
            fprintf(stderr, usage, argv[0]);
            exit(1);
        }
    }

    if (argc < 2) {
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }
    if (strcmp(argv[optind],"-")==0) {
        fin = stdin;
    } else {
        fin = fopen(argv[optind],"rb");
        if (fin == NULL) {
            fprintf(stderr, "Error opening input file: %s\n", argv[optind]);
            exit(1);
        }
    }
    
    // Set shiftHz at 2*Rs if no command line argument
    if (shiftHz == -1)
        shiftHz = 2*SymbolRate;
    log2m = log2(m);
    fmmod = new ngfmdmasync(frequency,SymbolRate,14,100); 	
    padgpio pad;
    pad.setlevel(7); // Set max power

    fprintf(stderr, "Frequency: %4.1f MHz Rs: %4.1f kHz Shift: %4.1f kHz M: %d \n", frequency/1E6, SymbolRate/1E3, shiftHz/1E3, m);
    
    if (one_zero_test == 0) { 
 
        while(running) {
            uint8_t tx_bits[log2m];
            int BytesRead = fread(tx_bits, sizeof(uint8_t), log2m, fin);
            if (BytesRead == log2m) {
                /* generate the symbol number from the bit stream, 
                   e.g. 0,1 for 2FSK, 0,1,2,3 for 4FSK */

                int sym = 0; int bit_i = 0;
                for(int i=m; i>>=1; ) {
                    uint8_t bit = tx_bits[bit_i] & 0x1;
                    sym = (sym<<1)|bit;
                    bit_i++;
                }
                float VCOfreqHz = shiftHz*sym;
                fmmod->SetFrequencySamples(&VCOfreqHz,1);
            }
            else
                running=false;
        }
        
    } else {

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
    delete fmmod;
    return 0;
}
	



