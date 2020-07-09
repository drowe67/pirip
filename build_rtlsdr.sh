# automate building of rtlsdr lib
git clone git://github.com/rtlsdrblog/rtl-sdr-blog.git
cd rtl-sdr-blog/
mkdir build_rtlsdr
cd build_rtlsdr
cmake ../ -DINSTALL_UDEV_RULES=ON
make
