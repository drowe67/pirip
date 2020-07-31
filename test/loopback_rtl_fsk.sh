#!/bin/bash -x
#
# Send test frames from Pi, receive and check on rtlsdr, using integrated rtl_fsk

source test/include.sh
results=mktemp

# kick off rx in background

rtl-sdr-blog/build_rtlsdr/src/rtl_fsk -g 1 -s $Fs -f $rx_freq - -n $(($Fs*$rx_secs)) -u $dash_host | codec2/build_linux/src/fsk_put_test_bits -q -p $passRxPackets  - 2>&1 | tee ${RESULTS} &
rx_pid=$!
# make sure rx is running
sleep 2

# send packets on Pi

ssh pi@raspberrypi "cd pirip/tx; ../../codec2/build_linux/src/fsk_get_test_bits - $numTxBits | sudo ./fsk_rpitx -"

# check results
wait ${rx_pid}
