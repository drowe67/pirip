<<<<<<< HEAD
# automate building of rtlsdr lib
=======
# automate building of rtlsdr library and applications
sudo apt update
sudo apt install libusb-1.0-0-dev git cmake
>>>>>>> b610864fdfdd03b6f9699434f1ba37cd5fd122b5
git clone git://github.com/rtlsdrblog/rtl-sdr-blog.git
cd rtl-sdr-blog/
mkdir build_rtlsdr
cd build_rtlsdr
cmake ../ -DINSTALL_UDEV_RULES=ON
make
