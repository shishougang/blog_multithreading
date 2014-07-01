#!/usr/bin/env python

import numpy as np
import matplotlib.pyplot as plt
import matplotlib
from matplotlib.ticker import FuncFormatter

def to_percent(y, position):
    # Ignore the passed in position. This has the effect of scaling the default
    # tick locations.
    s = str(100 * y)

    # The percent symbol needs escaping in latex
    if matplotlib.rcParams['text.usetex'] == True:
        return s + r'$\%$'
    else:
        return s + '%'

map = {
    '1.000000e-08' : 0,
    '3.160000e-08': 1,
    '1.000000e-07': 2,
    '3.160000e-07': 3,
    '1.000000e-06': 4,
    '3.160000e-06': 5,
    '1.000000e-05': 6,
    '3.160000e-05': 7,
    '1.000000e-04': 8,
    }

work_list = [[] for i in range(len(map))]

with open("lock_ben.txt") as file:
    for line in file:
        fields = [line.split(' ')][0]
        lock_interval = fields[1].split('=')[1]
        workdone = fields[3].split('=')[1]
        overshoot = fields[5].split('=')[1]
        work = int(workdone) - int(overshoot)
        work_list[map[lock_interval]].append(work)

reference = []
with open("reference.txt") as file:
    for line in file:
        fields = [line.split(' ')][0]
        workdone = fields[3].split('=')[1]
        overshoot = fields[5].split('=')[1]
        work = int(workdone) - int(overshoot)
        reference.append(work)


for j in range(len(map)):
    for i in range(len(work_list[0])):
        work_list[j][i] = work_list[j][i] * 1.0 / reference[i]


x = []
for i in range(len(work_list[1])):
    x.append(i * 0.005)

fig, ax = plt.subplots()

line_list = []
for i in range(len(map)):
    line_list.append(ax.plot(np.array(x), np.array(work_list[i]), linewidth=2))

ax.legend((line_list[0][0], line_list[1][0], line_list[2][0],
            line_list[3][0], line_list[4][0], line_list[5][0],
            line_list[6][0], line_list[7][0], line_list[8][0]),
            ('10 ns', '31.6 ns', '100 ns',
             '316 ns', '1 us', '3.16 us',
             '10 us', '31.6 us', '100 us'))

formatter = FuncFormatter(to_percent)
ax.xaxis.set_major_formatter(formatter)
ax.yaxis.tick_right()

x_ticks= [0.1 * i for i in range(11)]
ax.xaxis.set_ticks(x_ticks)
ax.grid(True, which='both')
plt.show()

