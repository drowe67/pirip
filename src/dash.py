#!/usr/bin/env python
#
#   rtl_fsk modem dashboard
#
# usage:
#   netcat -luk 8001 | ./dash.py

import json
import sys
import numpy as np
import matplotlib.pyplot as plt
import io

roll_s = 5
SNRestdB = []
norm_timing = [];

# unbuffered stdin
sys.stdin = io.open(sys.stdin.fileno())

for line in sys.stdin:

    data = json.loads(line)

    # update rolling SNRest
    SNRest_lin = data['SNRest_lin']
    tmp = 10*np.log10(SNRest_lin+1)
    SNRestdB.append(tmp)
    if len(SNRestdB) > roll_s:
        SNRestdB.pop(0)
    
    # update rolling norm timing
    latest_norm_timing = data['norm_rx_timing']
    norm_timing.extend(latest_norm_timing)
    one_sec = len(latest_norm_timing)
    if len(norm_timing) > roll_s*one_sec:
        norm_timing = norm_timing[one_sec:]
    
    plt.clf()
    plt.subplot(311)
    SfdB = data['SfdB']
    fsk_lower_kHz = np.array(data['fsk_lower_Hz'])/1000
    fsk_upper_kHz = np.array(data['fsk_upper_Hz'])/1000
    f_est_kHz = np.array(data['f_est_Hz'])/1000
    Fs_kHz = data['Fs_Hz']/1000
    f_axis_kHz = -Fs_kHz/2 + np.arange(len(SfdB))*Fs_kHz/len(SfdB)
    plt.plot(f_axis_kHz, SfdB)
    height = 10;
    mn = np.min(SfdB);
    for f in f_est_kHz:
        plt.plot(f, mn,"r+")
    plt.plot([fsk_lower_kHz, fsk_lower_kHz], [mn, mn+height],"g")
    plt.plot([fsk_upper_kHz, fsk_upper_kHz], [mn, mn+height],"g")
    plt.grid()
    plt.ylabel('Spectrum')
    
    plt.subplot(312)
    
    plt.plot(SNRestdB)
    plt.ylabel('SNR')
    mx = 10*np.ceil(np.max(SNRestdB)/10);
    plt.ylim([0, mx])
    plt.grid()
    plt.subplot(313)
    plt.plot(norm_timing)
    plt.ylabel('Timing')
    plt.ylim([-0.5, 0.5])
    
    plt.draw()
    plt.pause(0.01) # renders plot
