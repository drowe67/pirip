# Pi Radio IP

Minimal hardware IP over VHF/UHF Radio using RpiTx and RTLSDRs.

# Building

Everything is built locally.

## Codec 2

## RpiTx Transmitter

## RTLSDR Receiver

# Testing

Generate two tone test signal
Using rtl_sdr | csdr | fsk_demod
Using new integrated rtl_fsk
Test frames tx/rx setup


```
sudo apt update
sudo apt install libusb-1.0-0-dev git cmake
```

# Reading Further

1. [RpiTx](https://github.com/F5OEO/rpitx) - Radio transmitter software for Raspberry Pis
1. [rtlsdr driver](https://github.com/rtlsdrblog/rtl-sdr-blog) - Modified Osmocom drivers with enhancements for RTL-SDR Blog V3 units.
1. [Open IP over VHF/UHF](http://www.rowetel.com/?p=7207) - Blog post introducing this project
