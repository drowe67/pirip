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

SNRestdB = [];
sys.stdin = io.open(sys.stdin.fileno())
for line in sys.stdin:

    data = json.loads(line)

    SNRest_lin = data['SNRest_lin']
    tmp = 10*np.log10(SNRest_lin+1)
    SNRestdB.append(tmp)
    if len(SNRestdB) > 5:
        SNRestdB.pop(0)
        
    plt.clf()
    plt.subplot(211);
    Sf = data['Sf'];
    f_est_kHz = np.array(data['f_est_Hz'])/1000
    Fs_kHz = data['Fs_Hz']/1000
    f_axis_kHz = -Fs_kHz/2 + np.arange(len(Sf))*Fs_kHz/len(Sf)
    plt.plot(f_axis_kHz, Sf)
    for f in f_est_kHz:
        print(f)
        plt.plot([f, f], [0, 1],"r")
    
    plt.subplot(212);
    plt.plot(SNRestdB);
    plt.draw()
    plt.pause(0.1)
