#!/bin/bash -x
#
# Send test frames from Pi, receive and check on rtlsdr, using vanilla rtl_sdr

Fs=240000
Rb=10000
numTxPackets=100
bitsPerPacket=100
numTxBits=$(( ${numTxPackets}*${Rb}/${bitsPerPacket} ))
passRxPackets=$(( $numTxPackets-10 ))

results=mktemp

# kick off rx in background

tsecs=5; rtl-sdr-blog/build_rtlsdr/src/rtl_sdr -g 1 -s $Fs -f 144500000 - -n $(($Fs*$tsecs)) | codec2/build_linux/src/fsk_demod -d -p 24 2 $Fs $Rb - - | codec2/build_linux/src/fsk_put_test_bits -q -p $passRxPackets - 2>&1 | tee ${RESULTS} &
rx_pid=$!
sleep 2

# send packets on Pi

ssh pi@raspberrypi "cd pirip/tx; ../../codec2/build_linux/src/fsk_get_test_bits - $numTxBits | sudo ./fsk_rpitx -"

# check results
wait ${rx_pid}
