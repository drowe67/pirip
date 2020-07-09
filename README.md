# Pi Radio IP

Minimal hardware IP over VHF/UHF Radio using RpiTx and RTLSDRs.

# Building

This procedure builds everything locally.

## Codec 2

```
git clone https://github.com/drowe67/codec2.git
cd codec2 & mkdir build_linux && cd build_linux
cmake ../ && make
```

## RpiTx Transmitter

ssh into your Pi, then build the RpiTx library and ```fsk_rpitx``` FSK modulator application:
```
$ git clone https://github.com:drowe67/pirip.git
$ ./build_rpitx.sh
$ cd tx && make
```
   
## RTLSDR Receiver

```
$ sudo apt update
$ sudo apt install libusb-1.0-0-dev git cmake
$ ./build_rtlsdr.sh

```

# Reading Further

[Codec 2 FSK Modem](https://github.com/drowe67/codec2/blob/master/README_fsk.md)
[RpiTx](https://github.com/F5OEO/rpitx) - Radio transmitter software for Raspberry Pis
[rtlsdr driver](https://github.com/rtlsdrblog/rtl-sdr-blog) - Modified Osmocom drivers with enhancements for RTL-SDR Blog V3 units. 
[Open IP over VHF/UHF](http://www.rowetel.com/?p=7207) - Blog post introducing this project
