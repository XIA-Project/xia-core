#!/usr/bin/python
import sys
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

import operator as op
def ncr(n, r):
    r = min(r, n-r)
    if r == 0: return 1
    numer = reduce(op.mul, xrange(n, n-r, -1))
    denom = reduce(op.mul, xrange(1, r+1))
    return numer//denom

MAX = 100
X = [x for x in range(0,MAX)]
y = [100]
for i in range(1, MAX):
    n = 0
    for j in range(1, MAX):
        try:
            n += j * ncr(MAX-1-j,i-1)
        except:
            pass
    y.append(float(n) / ncr(MAX-1,i))
print X,y

plt.clf()
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_xlabel('Percentage of Path Upgraded')
ax.set_ylabel('Expected Distance as Percentage of Path')

plt.plot(X, y)
#ax.axis([0, xmax[i], 0, 100])
plt.savefig('/home/cmu_xia/incentives.png')
