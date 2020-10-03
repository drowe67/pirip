# Pi Radio IP

Minimal hardware IP over VHF/UHF Radio using RpiTx and RTLSDRs [1].

![10 kbit/s Spectrum](doc/rpitx_spectrum_10kbps.png)

# Project Plan and Status

Currently working on M4 -Over The Air (OTA) tests.

| Milestone | Description |
| --- | --- |
| M1 | ~~Proof of Concept Physical Layer~~ |
| M2 | ~~Git repo for project, integrated tx and rx applications~~ |
| M3 | ~~Simple GUI Dashboard that can be used to tune and debug link~~ |
| M4 | ~~first OTA tests using uncoded modem~~ |
| M5 | ~~Pi running Tx and Rx~~ |
| M6 | ~~Add LDPC FEC to waveform~~ |
| M7 | Bidirectional half duplex Tx/Rx on single Pi |
| M8 | TAP/TUN integration and demo IP link |
| M9 | Document how to build simple wire antennas |

# Building

This procedure builds everything locally, so won't interfere with any installed versions of the same software.

## RpiTx Transmitter

ssh into your Pi, then:
```
$ git clone https://github.com/drowe67/pirip.git
$ cd pirip
$ ./build_codec2.sh
$ ./build_rpitx.sh
$ cd tx && make
```

## RTLSDR FSK Receiver

On your laptop/PC:
```
$ sudo apt update
$ sudo apt install libusb-1.0-0-dev git cmake
$ ./build_codec2.sh
$ ./build_csdr.sh
$ ./build_rtlsdr.sh
```

# Testing

1. Transmit two tone test signal for Pi:
   ```
   pi@raspberrypi:~/pirip/tx $ sudo ./fsk_rpitx -t /dev/null
   ```
1. Transmit test frames from Pi for 60 seconds:
   ```
   pi@raspberrypi:~/pirip/tx $ ../codec2/build_linux/src/fsk_get_test_bits - 600000 | sudo ./fsk_rpitx -
   ```
1. Receive test frames on x86 laptop for 5 seconds (vanilla rtl_sdr):
   ```
   ~/pirip$ Fs=240000; librtlsdr/build_rtlsdr/src/rtl_sdr -g 49 -s $Fs -f 144490000 - | codec2/build_linux/src/fsk_demod --fsk_lower 500 --fsk_upper 25000 -d -p 24 2 240000 10000 - - | codec2/build_linux/src/fsk_put_test_bits -

1.  Receive test frames on x86 laptop for 5 seconds (vanilla rtl_sdr at Fs=1.8MHz):
   ```
   Fs=1800000; ./src/rtl_sdr -g 49 -s $Fs -f 144500000 - | csdr convert_u8_f | csdr fir_decimate_cc 45 | csdr convert_f_s16 | ../../codec2/build_linux/src/fsk_demod --fsk_lower 500 -c 2 40000 1000 - - | ../../codec2/build_linux/src/fsk_put_test_bits -

   ```
1. Receive test frames on x86 laptop for 5 seconds (integrated rtl_fsk):
   ```
   ~/pirip$  Fs=240000; tsecs=5; ./librtlsdr/build_rtlsdr/src/rtl_fsk -g 49 -f 144490000 - -n $(($Fs*$tsecs)) | codec2/build_linux/src/fsk_put_test_bits -
   ```
   Note this is tuned about 10kHz low, to put the two tones above the rtl_sdr DC line.  
1. Demod GUI Dashboard. Open a new console and start `dash.py`:
   ```
   ~/pirip$ netcat -luk 8001 | ./src/dash.py
   ```
   In another console start the FSK demod:
   ```
   ~/pirip$ Fs=240000; tsecs=20; ./librtlsdr/build_rtlsdr/src/rtl_fsk -g 1 -f 144490000 - -n $(($Fs*$tsecs)) -u localhost | codec2/build_linux/src/fsk_put_test_bits -
   ```
   ![](doc/dash.png)
1. Automated loopback tests.  Connect your Pi to your RTLSDR via a 60dB attenuator

   Using vanilla `rtl_sdr`:
   ```
   ./test/loopback_rtl_sdr.sh
   ```
   Using integrated `rtl_fsk`:
   ```
   ./test/loopback_rtl_fsk.sh
   ```
   You can monitor `loopback_rtl_fsk.sh` using `dash.py` as above.

1. Using a HackRF as a transmitter, useful for bench testing the link.  The relatively low levels out of the HackRF make MDS testing easier compared to attenuating the somewhat stronger signal from the Pi.
   This example generates 1000 bit/s FSK with a 2000Hz shift:
   ```
   cd codec2/build_linux/src
   ./fsk_get_test_bits - 60000 | ./fsk_mod -c -a 30000 2 40000 1000 1000 2000 - - | ../misc/tlininterp - t.iq8 100 -d -f
   ```
   The output samples are at a sample rate of 4MHz, and a frequency offset of +1 MHz.  They can be played out of the HackRF with:
   ```
   hackrf_transfer -t t.iq8 -s 4E6 -f 143.5E6
   ```
   The signal will be centred on 144.5 MHz (143.5 + 1 MHz offset).

   You can receive it with:
   ```
   ./rtl_fsk -w 500E3 -e ff8 -r 1000 -f 144490000 - -u localhost | ~/pirip/codec2/build_linux/src/fsk_put_test_bits -
   ```

1. Noise Figure Testing

   Connect a signal generator to the input of the RTLSDR.  Set the frequency to 144.5MHz, and amplitude to -100dBm.

   The following command pipes the RTL output to an Octave script to measure noise figure.  You need the CSDR tools and Octave installed:
   ```
   $ cd ~/pirip/rtl-sdr-blog/build_rtlsdr/src
   $ ./rtl_sdr -g 50 -s 2400000 -f 144.498E6 - | csdr convert_u8_f | csdr fir_decimate_cc 50 | csdr convert_f_s16 | octave --no-gui -qf ~/pirip/codec2/octave/nf_from_stdio.m 48000 complex

   ```
   A few Octave plot windows will pop up.  Adjust your signal generator frequency so the sine wave is between 2000 and 4000, the
   script will print the Noise Figure (NF).  Around 6.5 dB was obtained using RTL-SDR.COM V3s using"-g 50"
   
   See also codec2/octave/nf_from_stdio.m and [Measuring SDR Noise Figure in Real Time](http://www.rowetel.com/?page_id=6172).

1. Running Rx on Pi:  This example 10 kbit/s, dashboard running on laptop 192.168.1.100
   ```
   ./rtl_fsk -s 2400000 -a 80000 -w 500E3 -e ff8 -r 10000 -f 144490000 - -u 192.168.1.100 | ~/pirip/codec2/build_linux/src/fsk_put_test_bits -

   ```

1. FSK with LDPC and framer at 1000 bit/s.  On the Pi Tx, we use an external source of test frames:
   ```
   $ cd ~/pirip/tx
   $ ../codec2/build_linux/src/fsk_get_test_bits - 2560 256 | sudo ./fsk_rpitx - --code H_256_512_4 -r 1000 -s 1000
   ```
   Laptop Rx:
   ```
   $ cd ~/pirip/librtlsdr/build_rtlsdr
   $ ./src/rtl_fsk -g 49 -f 144490000 - -r 1000 --code  H_256_512_4 -v -u localhost > /dev/null
   ```
   In this example we aren't counting errors in the received frames, but you can get some indication from the
   number of "iters" - if it's just 1 the FEC decoded isn't working very hard.
   
1. FSK with LDPC and framer at 10000 bit/s, internal test frames.  On the Pi Tx:
   ```
   $ sudo ./fsk_rpitx /dev/zero --code H_256_512_4 -r 10000 -s 10000 --testframes 10
   ```
   Laptop Rx:
   ```
   $ cd ~/pirip/librtlsdr/build_rtlsdr
   $ ./src/rtl_fsk -g 1 -f 144490000 - -a 100000 -r 10000 --code  H_256_512_4 -v -u localhost --testframes > /dev/null
   ```
   When the Pi transmits a burst, you'll see something like:
   ```
   721 nbits:  28 state: 1 uw_loc:  58 uw_err:  5 bad_uw: 0 snrdB:  7.4 eraw:  36 ecdd:   0 iter:   9 pcc: 256 rxst: -BS-
   722 nbits:  34 state: 1 uw_loc:  58 uw_err:  2 bad_uw: 0 snrdB:  6.4 eraw:  44 ecdd:   0 iter:   9 pcc: 256 rxst: -BS-
   723 nbits:  40 state: 1 uw_loc:  58 uw_err:  4 bad_uw: 0 snrdB:  7.1 eraw:  46 ecdd:   0 iter:  10 pcc: 256 rxst: -BS-
   724 nbits:  46 state: 1 uw_loc:  58 uw_err:  3 bad_uw: 0 snrdB:  7.3 eraw:  44 ecdd:   0 iter:   7 pcc: 256 rxst: -BS-
   725 nbits:   2 state: 1 uw_loc:  58 uw_err:  2 bad_uw: 0 snrdB:  6.8 eraw:  30 ecdd:   0 iter:   5 pcc: 256 rxst: -BS-
   726 nbits:   8 state: 1 uw_loc:  58 uw_err:  1 bad_uw: 0 snrdB:  6.8 eraw:  55 ecdd:   0 iter:  14 pcc: 256 rxst: -BS-
   727 nbits:  14 state: 1 uw_loc:  58 uw_err:  3 bad_uw: 0 snrdB:  7.6 eraw:  64 ecdd:   0 iter:  15 pcc: 250 rxst: EBS-
   728 nbits:  20 state: 1 uw_loc:  58 uw_err:  1 bad_uw: 0 snrdB:  7.5 eraw:  42 ecdd:   0 iter:   8 pcc: 256 rxst: -BS-
   729 nbits:  26 state: 1 uw_loc:  58 uw_err:  4 bad_uw: 0 snrdB:  6.5 eraw:  52 ecdd:   0 iter:  15 pcc: 253 rxst: EBS-
   ```
   In this example the uncoded (raw) errors are getting close to 10%, where this particular code breaks down.  You can tell the
   FEC decoder is working pretty hard as the number of iterations is close to the maximum of 15, and the parity checks
   don't always match.  However the coded errors are still zero, although we only received 9/10 packets transmitted.

1. Calibration of FSK power.  We subsitute a sine wave of the same amplitude (power) as the FSK signal.

   This line generates a sine wave (-t) option which just sets all bits to 0 before FSK modulation:
   ```
   $ ./src/fsk_get_test_bits - 60000 | ./src/fsk_mod -t -c -a 30000 2 40000 1000 1000 2000 - - | ./misc/tlininterp - hackrf_tone.iq8 100 -d -f
   ```
   Then play using HackRF and measure signal level:
   ```
   $ hackrf_transfer -t hackrf_tone.iq8 -s 4E6 -f 143.5E6
   ```

   We can generate a 1000 bit/s FSK LDPC signal comprising 3 packets with the same power as the test sine wave above using:
   ```
   $ ./src/freedv_data_raw_tx -c --testframes 3 --Fs 40000 --Rs 1000 --tone1 1000 --shift 1000 -a 30000 FSK_LDPC /dev/zero - | ./misc/tlininterp - hackrf_rs1000.iq8 100 -d -f
   $ hackrf_transfer -t hackrf_rs1000.iq8 -s 4E6 -f 143.5E6
   ```
   
# Reading Further

1. [Open IP over VHF/UHF 1](http://www.rowetel.com/?p=7207) - Blog post introducing this project
1. [Open IP over VHF/UHF 2](http://www.rowetel.com/?p=7334) - Second blog post on uncoded OTA tests
1. [Previous Codec 2 PR discussing this project](https://github.com/drowe67/codec2/pull/125)
1. [Codec 2 FSK Modem](https://github.com/drowe67/codec2/blob/master/README_fsk.md)
1. [RpiTx](https://github.com/F5OEO/rpitx) - Radio transmitter software for Raspberry Pis
1. [rtlsdr driver](https://github.com/librtlsdr/librtlsdr) - Our rtlsdr driver is forked from this fine repo. 
1. [Measuring SDR Noise Figure in Real Time](http://www.rowetel.com/?page_id=6172)