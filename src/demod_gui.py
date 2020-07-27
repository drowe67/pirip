#!/usr/bin/env python
#
#   Plot fsk_demod statistic outputs.
#
#   Copyright (C) 2018  Mark Jessop <vk5qi@rfhead.net>
#   Released under GNU GPL v3 or later
#
# usage:
#   netcat -luk 8001 | ./demod_gui.py

import json
import sys
import numpy as np
import matplotlib.pyplot as plt
import io

SNRest = [];
sys.stdin = io.open(sys.stdin.fileno())
for line in sys.stdin:
    if line[0] != '{':
        continue

    try:
        data = json.loads(line)
    except Exception as e:
        continue

    SNRest.append(data['SNRest'])
    if len(SNRest) > 5:
        SNRest.pop(0)
        
    plt.clf()
    plt.subplot(211);
    Sf = data['Sf']; plt.plot(Sf)
    f_est = data['f_est']
    x_zero = int(len(Sf)/2)
    for f in range(len(f_est)):
        plt.plot([f_est[f]+x_zero, f_est[f]+x_zero], [0, 1],"r")
    
    plt.subplot(212);
    plt.plot(SNRest);
    plt.draw()
    plt.pause(0.1)
