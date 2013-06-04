#!/usr/bin/python

import sys
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

PING_BIN_SIZE = 10
PING_MAX = 400
TR_BIN_SIZE = 1
TR_MAX = 40

pings = [[],[],[],[]]
hops = [[],[],[],[]]
types = ['6RD', '4ID', 'SDN']

if len(sys.argv) < 3:
    print 'usage: %s [stats-tunneling.txt] [stats-4id.txt]' % (sys.argv[0])
    sys.exit(-1)

pairs = open(sys.argv[2],'r').read().split('\n')[:-1]
pairs = [eval(pair.split('):')[1]) for pair in pairs]
for pair in pairs:
    for t in range(3):
        try:
            ab = '%s-AB' % types[t]
            ba = '%s-BA' % types[t]
            gg = '%s-GG' % types[t]
            ag = '%s-AG' % types[t]
            bg = '%s-BG' % types[t]
            if pair[t][ab][2] == '-1.000':
                continue
            if pair[t][ba][2] == '-1.000':
                continue
            if pair[t][gg][3] == -1:
                continue
            if pair[t][ag][2] == '-1.000':
                continue
            if pair[t][bg][2] == '-1.000':
                continue

            xp = (eval(pair[t][ab][2]) + eval(pair[t][ba][2]))/2
            p = eval(pair[t][ag][2]) + eval(pair[t][gg][2]) + eval(pair[t][bg][2])

            pings[t+1].append(p)
            hops[t+1].append(1 + pair[t][gg][3] + 1)
        except:
            pass


pairs = open(sys.argv[1],'r').read().split('\n')[:-1]
pairs = [(eval(pair.split(':')[0]), eval(pair.split(':')[1])) for pair in pairs]
for pair in pairs:
    try:
        if pair[1][0][3] < 3 or pair[1][1][3] < 3:
            continue
        if pair[1][3][2] == '-1.000' or pair[1][2][2] == '-1.000':
            continue
        pings[0].append((eval(pair[1][3][2]) + eval(pair[1][2][2])) /2)
        hops[0].append(pair[1][0][3] + pair[1][1][3] + pair[1][2][3])
    except:
        pass

for t in range(4):
    pings[t] = sorted(pings[t])
    hops[t] = sorted(hops[t])

# remove outlies
for i in range(4):
    pings[i] = [p for p in pings[i] if p < PING_MAX]

print len(pings[0]), len(pings[1]), len(pings[2]), len(pings[3])

MAX = max(pings[0][-1], pings[1][-1], pings[2][-1], pings[3][-1]) + PING_BIN_SIZE
MAX = int(MAX - (MAX % PING_BIN_SIZE))
fig = plt.figure()
plt.title('Latency using Different Deployment Mechanisms')
ax = fig.add_subplot(111)
ax.set_xlabel('Ping Time (ms)')
ax.set_ylabel('Percentage of Ping Times')
X = [PING_BIN_SIZE*x for x in range(0,MAX/PING_BIN_SIZE)]
for pt in pings:
    Y = {}
    for x in X:
        Y[x] = 0.0
    for ping in pt:
        Y[ping - (ping % PING_BIN_SIZE)] += (1.0/len(pt))

    p = sorted(zip(Y.keys(), Y.values()))
    plt.plot(zip(*p)[0], 100*np.cumsum(zip(*p)[1]))
plt.legend(('Tunneling', '6RD/6to4', '4ID', 'Smart 4ID'),
           'lower right')
ax.axis([0, PING_MAX, 0, 100])
plt.savefig('/home/cmu_xia/ping.png')


plt.clf()
fig = plt.figure()
plt.title('Path Stretch using Different Deployment Mechanisms')
ax = fig.add_subplot(111)
ax.set_xlabel('Hop Count')
ax.set_ylabel('Percentage of Hop Counts')
X = [TR_BIN_SIZE*x for x in range(0,TR_MAX/TR_BIN_SIZE)]
for ht in hops:
    Y = {}
    for x in X:
        Y[x] = 0.0
    for hop in ht:
        Y[hop - (hop % TR_BIN_SIZE)] += (1.0/len(ht))

    p = sorted(zip(Y.keys(), Y.values()))
    plt.plot(zip(*p)[0], 100*np.cumsum(zip(*p)[1]))
plt.legend(('Tunneling', '6RD/6to4', '4ID', 'Smart 4ID'),
           'lower right')
ax.axis([0, TR_MAX, 0, 100])
plt.savefig('/home/cmu_xia/hops.png')
