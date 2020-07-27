#!/usr/bin/env python
#
#   Plot fsk_demod statistic outputs.
#
#   Copyright (C) 2018  Mark Jessop <vk5qi@rfhead.net>
#   Released under GNU GPL v3 or later
#

import json
import sys
import numpy as np
import matplotlib.pyplot as plt
import io

sys.stdin = io.open(sys.stdin.fileno())
for line in sys.stdin:
    if line[0] != '{':
        continue

    try:
        data = json.loads(line)
    except Exception as e:
        continue

    plt.clf()
    plt.plot(data['Sf'])
    plt.draw()
    plt.pause(0.1)
