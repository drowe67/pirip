#!/bin/bash -x
# Automate building of rtlsdr library and applications
PIRIP_DIR=$(pwd)
git clone https://github.com/drowe67/librtlsdr.git
cd librtlsdr
git checkout development
mkdir -p build_rtlsdr
cd build_rtlsdr
cmake ../ -DINSTALL_UDEV_RULES=ON -DCODEC2_BUILD_DIR=${PIRIP_DIR}/codec2/build_linux -DCSDR_BUILD_DIR=${PIRIP_DIR}/csdr
make rtl_sdr rtl_fsk
