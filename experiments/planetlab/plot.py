#!/usr/bin/python

import sys
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

PING_BIN_SIZE = 20
PING_MAX = 5000
TR_BIN_SIZE = 1
TR_MAX = 60

if len(sys.argv) < 2:
    print 'usage: %s [stats.txt]' % (sys.argv[0])
    sys.exit(-1)

pairs = open(sys.argv[1],'r').read().split('\n')[:-1]
pairs = [(eval(pair.split(':')[0]), eval(pair.split(':')[1])) for pair in pairs]
pings = []
for pair in pairs:
    try:
        pings.append((eval(pair[1][3][2]) + eval(pair[1][2][2])) /2)
    except:
        pass
    
hops = []
for pair in pairs:
    try:
        hops.append(pair[1][0][3] + pair[1][1][3] + pair[1][2][3])
    except:
        pass

print pings, hops
print len(pings)

X = [PING_BIN_SIZE*x for x in range(0,PING_MAX/PING_BIN_SIZE)]
Y = {}
for x in X:
    Y[x] = 0.0
for ping in pings:
    Y[ping - (ping % PING_BIN_SIZE)] += (1.0/len(pings))   

p = sorted(zip(Y.keys(), Y.values()))

plt.plot(zip(*p)[0], np.cumsum(zip(*p)[1]))
plt.savefig('/home/cmu_xia/ping.png')


X = [TR_BIN_SIZE*x for x in range(0,TR_MAX/TR_BIN_SIZE)]
Y = {}
for x in X:
    Y[x] = 0.0
for hop in hops:
    Y[hop - (hop % TR_BIN_SIZE)] += (1.0/len(hops))   

p = sorted(zip(Y.keys(), Y.values()))

plt.clf()
plt.plot(zip(*p)[0], np.cumsum(zip(*p)[1]))
plt.savefig('/home/cmu_xia/hops.png')
