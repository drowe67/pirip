# automate building of rtlsdr library and applications
sudo apt update
sudo apt install libusb-1.0-0-dev git cmake
git clone git://github.com/rtlsdrblog/rtl-sdr-blog.git
cd rtl-sdr-blog/
mkdir build_rtlsdr
cd build_rtlsdr
cmake ../ -DINSTALL_UDEV_RULES=ON
make
