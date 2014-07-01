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


work_list = [[] for i in range(5)]

with open("contention.txt") as file:
    for line in file:
        fields = [line.split(' ')][0]
        thread_id = fields[0].split('=')[1]
        workdone = fields[3].split('=')[1]
        overshoot = fields[5].split('=')[1]
        work = int(workdone) - int(overshoot)
        work_list[int(thread_id)].append(work)


for i in range(len(work_list[1])):
    work_list[2][i] = work_list[2][i] * 1.0 / work_list[1][i]
    work_list[3][i] = work_list[3][i] * 1.0 / work_list[1][i]
    work_list[4][i] = work_list[4][i] * 1.0 / work_list[1][i]
    work_list[1][i] = work_list[1][i] * 1.0 / work_list[1][i]

x = []
for i in range(len(work_list[1])):
    x.append(i * 0.005)

fig, ax = plt.subplots()

line1 = ax.plot(np.array(x), np.array(work_list[1]), linewidth=2)
line2 = ax.plot(np.array(x), np.array(work_list[2]), linewidth=2)
line3 = ax.plot(np.array(x), np.array(work_list[3]), linewidth=2)
line4 = ax.plot(np.array(x), np.array(work_list[4]), linewidth=2)
ax.legend( (line1[0], line2[0], line3[0], line4[0]),
           ('thread1', 'thread2', 'thread3', 'thread4'))

formatter = FuncFormatter(to_percent)
ax.xaxis.set_major_formatter(formatter)
ax.yaxis.tick_right()
ax.grid(True)
plt.show()

