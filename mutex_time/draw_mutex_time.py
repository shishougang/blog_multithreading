#!/usr/bin/env python
import numpy as np
import matplotlib.pyplot as plt

N = 1
i5 = (34.2)

ind = np.arange(N)  # the x locations for the groups
width = 0.35       # the width of the bars

fig, ax = plt.subplots()
rects1 = ax.barh(ind, i5, width, color='r')

i7 = (21.0)
rects2 = ax.barh(ind+width, i7, width, color='y')
plt.subplots_adjust(left=0.115, right=0.88)

ax.set_ylabel('phtread mutex')
ax.set_yticks(ind+width)
ax.set_yticklabels( '' )
ax.set_xlim(0, 60)
ax.set_ylim(0, 1.4)
ax.legend( (rects1[0], rects2[0]),
           ('2.40GHz i5 X64-Ubuntu', '2.93GHz i7 X86-Ubuntu',),
           loc='center right')

ax.text(38, 0.15, '34.2 ns', ha='center', va='bottom')
ax.text(24, 0.5, '21.0 ns', ha='center', va='bottom')
plt.show()
