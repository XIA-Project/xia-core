#!/usr/bin/python

import sys
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

PING_BIN_SIZE = 20
PING_MAX = 1000 #5000
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
            pings[t+1].append((eval(pair[t]['%s-AB' % types[t]][2]) + eval(pair[t]['%s-BA' % types[t]][2]))/2)
            hops[t+1].append(1 + pair[t]['%s-GG' % types[t]][3] + 1)
            if pings[t+1][-1] < 50:
                print pair[t]

        except:
            pass


pairs = open(sys.argv[1],'r').read().split('\n')[:-1]
pairs = [(eval(pair.split(':')[0]), eval(pair.split(':')[1])) for pair in pairs]
for pair in pairs:
    try:
        pings[0].append((eval(pair[1][3][2]) + eval(pair[1][2][2])) /2)
        hops[0].append(pair[1][0][3] + pair[1][1][3] + pair[1][2][3])
#         if pings[0][-1] < 50:
#             print pair
    except:
        pass

for t in range(4):
    pings[t] = sorted(pings[t])
    hops[t] = sorted(hops[t])

print len(pings[0]), len(pings[1]), len(pings[2]), len(pings[3])

# remove outlies
for i in range(4):
    pings[i] = [p for p in pings[i] if p < 1000]

#print pings[0][len(pings[0])/2]
#print pings[3][len(pings[3])/2]

print hops[1]
print hops[2]
print hops[3]
print pings[1]
print pings[2]
print pings[3]


fig = plt.figure()
plt.title('Latency using Different Deployment Mechanisms')
ax = fig.add_subplot(111)
ax.set_xlabel('Ping Time (ms)')
ax.set_ylabel('Percentage of Ping Times')
X = [PING_BIN_SIZE*x for x in range(0,PING_MAX/PING_BIN_SIZE)]
for pt in pings:
    Y = {}
    for x in X:
        Y[x] = 0.0
    for ping in pt:
        Y[ping - (ping % PING_BIN_SIZE)] += (1.0/len(pt))

    p = sorted(zip(Y.keys(), Y.values()))
    plt.plot(zip(*p)[0], 100*np.cumsum(zip(*p)[1]))
plt.legend(('Tunneling', '6RD/6to4', '4ID', '4ID+SDN'),
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
