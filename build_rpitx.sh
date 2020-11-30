# automate building of RpiTx library
git clone https://github.com/F5OEO/librpitx
cd librpitx/src
make
cd ../../
echo "To the end of /boot/config.txt add"
echo "  gpu_freq=250"
echo "  force_turbo=1"
echo "and reboot your Pi"
