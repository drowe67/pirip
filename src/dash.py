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
        norm_timing = norm_timing[one_sec:-one_sec]
    
    plt.clf()
    plt.subplot(311)
    Sf = data['Sf']
    f_est_kHz = np.array(data['f_est_Hz'])/1000
    Fs_kHz = data['Fs_Hz']/1000
    f_axis_kHz = -Fs_kHz/2 + np.arange(len(Sf))*Fs_kHz/len(Sf)
    plt.plot(f_axis_kHz, Sf)
    for f in f_est_kHz:
        print(f)
        plt.plot([f, f], [0, 1],"r")
    
    plt.subplot(312)
    plt.plot(SNRestdB)
    plt.subplot(313)
    plt.plot(norm_timing)
    
    plt.draw()
    plt.pause(0.01) # renders plot
