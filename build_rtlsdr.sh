#!/bin/bash -x
# automate building of rtlsdr library and applications
PIRIP_DIR=$(pwd)
git clone https://github.com/drowe67/rtl-sdr-blog.git
cd rtl-sdr-blog
mkdir -p build_rtlsdr
cd build_rtlsdr
cmake ../ -DINSTALL_UDEV_RULES=ON -DCODEC2_BUILD_DIR=${PIRIP_DIR}/codec2/build_linux
make rtl_fsk
