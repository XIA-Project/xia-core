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
TR_MIN = 14
BROWSER_BIN_SIZE = 100
BROWSER_MAX = 5000
DETECTION_BIN_SIZE = 20
DETECTION_MAX = 400

# Tunnels: 2*RTT to local gateway; new tunnel setup
# 6RD: 2*RTT to local gateway; BGP/OSPF prop (remember, Egress is sep from Ingress in 6RD)
# 4ID: 2*RTT to local gateway; Select new egress; (OR BGP/OSPF prop (remember, Egress is sep from Ingress in 4ID))
# SDN: 2*RTT to local gateway; Select new egress; (OR BGP/OSPF prop (remember, Egress is sep from Ingress in SDN))
local = [[],[],[],[]]

# Tunnels: 2*RTT to remote HOST
# 6RD: 2*RTT to remote gateway
# 4ID: 2*RTT to remote gateway
# SDN: 2*RTT to remote gateway
detection = [[],[],[],[]]

# Tunnels: Wait for remote host to reconfigure tunnel; wait for BGP to prop on tunnel; must restablish connection (?)
# 6RD: Wait for remote host to update IP/DNS; wait for DNS update to prop; must restablish connection
# 4ID: 2*RTT to remote gateway; pick new 4ID ingress; (remote updates DNS)
# SDN: 2*RTT to remote gateway; pick new 4ID ingress; (remote updates DNS)
remote = [[],[],[],[]]


pings = [[],[],[],[]]
hops = [[],[],[],[]]
browsers = [[],[],[],[]]
types = ['6RD', '4ID', 'SDN']
titles = ['Latency using Different Deployment Mechanisms', 'Path Stretch using Different Deployment Mechanisms', 'Simple Webpage Latency', 'Detection Time for Remote Failures']
binsize = [PING_BIN_SIZE, TR_BIN_SIZE, BROWSER_BIN_SIZE, DETECTION_BIN_SIZE]
xmax = [PING_MAX, TR_MAX, BROWSER_MAX, DETECTION_MAX]
xmin = [0, TR_MIN, 0, 0]
xlabels = ['Ping Time (ms)', 'Hop Count', 'Time to Completion (ms)', 'Time (ms)']
ylabels = ['Percentage of Ping Times', 'Percentage of Hop Counts', 'Percentage of Browser Experiments', 'Percentage of Tests']
legends = ('6in4', '6RD', '4ID', 'Smart 4ID')
outputs = ['/home/cmu_xia/ping.pdf', '/home/cmu_xia/hops.pdf', '/home/cmu_xia/browsers.pdf', '/home/cmu_xia/detection.pdf']
colors = ['b','g','r','c']

if len(sys.argv) < 3:
    print 'usage: %s [stats-tunneling.txt] [stats-4id.txt]' % (sys.argv[0])
    sys.exit(-1)

pairs = open(sys.argv[2],'r').read().split('\n')[:-1]
pairs = [eval(pair.split('):')[1]) for pair in pairs]
for pair in pairs:
    for t in range(3):
        # pair: [6RD data, 4ID data, SDN data]
        # -->: {'type-AB': data, 'type-BA': data, 'type-AG': data, 'type-BG': data, 'type-GG': data, 'browser': bdata}
        # -->-->: [source, dest, ping, hops] (data)
        # -->-->: [source, dest, latency] (bdata)

        ab = '%s-AB' % types[t]
        ba = '%s-BA' % types[t]
        gg = '%s-GG' % types[t]
        ag = '%s-AG' % types[t]
        bg = '%s-BG' % types[t]
        browser = 'browser'

        try:
            if pair[t][ag][2] != '-1.000' and pair[t][gg][2] != '-1.000' \
                    and pair[t][bg][2] != '-1.000':
                p = eval(pair[t][ag][2]) + eval(pair[t][gg][2]) + eval(pair[t][bg][2])
                #pings[t+1].append(p)
        except:
            pass

        try:
            if pair[t][ab][2] != '-1.000' and pair[t][ba][2] != '-1.000':
                p = (eval(pair[t][ab][2]) + eval(pair[t][ba][2])) /2
                pings[t+1].append(p)
        except:
            pass

        try:
            if pair[t][ag][3] != 1 and pair[t][gg][3] != -1 and pair[t][bg][3] != -1 and pair[t][gg][3] > 1:
                hops[t+1].append(pair[t][ag][3] + pair[t][gg][3] + pair[t][bg][3])
                if hops[t+1][-1] < 10:
                    print pair
        except:
            pass
        
        try:
            browsers[t+1].append(eval(pair[t][browser][2])*1000)
        except:
            pass
        
        try:
            if pair[t][ag][2] != '-1.000' and pair[t][gg][2] != '-1.000':
                ptoABG = eval(pair[t][ag][2])+eval(pair[t][gg][2])
                detection[t+1].append(ptoABG*2)
        except:
            pass

        try:
            if pair[t][bg][2] != '-1.000' and pair[t][gg][2] != '-1.000':
                ptoBAG = eval(pair[t][bg][2])+eval(pair[t][gg][2])
                detection[t+1].append(ptoBAG*2)
        except:
            pass

pairs = open(sys.argv[1],'r').read().split('\n')[:-1]
pairs = [eval(pair.split('):')[1]) for pair in pairs]
for pair in pairs:
    ab = 'AB'
    ba = 'BA'
    ag = 'backbone-A'
    bg = 'backbone-B'
    browser = 'browser'
    try:
        if pair[ab][2] != '-1.000' and pair[ba][2] != '-1.000':
            pings[0].append((eval(pair[ba][2]) + eval(pair[ab][2])) /2)
    except:
        pass

    try:
        if pair[ag][3] > 2 and pair[bg][3] > 2:
            hops[0].append(pair[ag][3] + pair[bg][3] + pair[ab][3]-2)
    except:
        pass

    try:
        browsers[0].append(eval(pair[browser][2])*1000)
    except:
        pass

    try:
        if pair[ab][2] != '-1.000':
            detection[0].append(eval(pair[ab][2])*2)
    except:
        pass

    try:
        if pair[ba][2] != '-1.000':
            detection[0].append(eval(pair[ba][2])*2)
    except:
        pass

for t in range(4):
    pings[t] = sorted(pings[t])
    hops[t] = sorted(hops[t])
    browsers[t] = sorted(browsers[t])
    detection[t] = sorted(detection[t])

#####################
#for t in range(4):
#    if len(browsers[t]) == 0:
#        browsers[t] = [BROWSER_MAX-1]
####################

data = [pings, hops, browsers, detection]
lines = []
for i in range(4):
    # remove outlies
    for j in range(4):
        data[i][j] = [p for p in data[i][j] if p < xmax[i]]
    print len(data[i][0]), len(data[i][1]), len(data[i][2]), len(data[i][3])

    MAX = max(data[i][0][-1], data[i][1][-1], data[i][2][-1], data[i][3][-1]) + binsize[i]
    MAX = int(MAX - (MAX % binsize[i]))
    plt.clf()
    fig = plt.figure()
    #plt.title(titles[i])
    ax = fig.add_subplot(111)
    ax.set_xlabel(xlabels[i])
    ax.set_ylabel(ylabels[i])
    X = [binsize[i]*x for x in range(0,MAX/binsize[i])]
    for pt in data[i]:
        Y = {}
        for x in X:
            Y[x] = 0.0
        for p in pt:
            Y[p - (p % binsize[i])] += (1.0/len(pt))

        p = sorted(zip(Y.keys(), Y.values()))
        lines.append(plt.plot(zip(*p)[0], 100*np.cumsum(zip(*p)[1]), color=colors[data[i].index(pt)]))
        

        p2 = zip(np.cumsum(zip(*p)[1]),zip(*p)[0])
        lside = (0,)
        rside = (1,)
        for j in p2:
            if j[0] > lside[0] and j[0] <= .5: lside = j
            if j[0] < rside[0] and j[0] >= .5: rside = j

        m = (rside[0]-lside[0])/(rside[1]-lside[1])
        b = ((rside[0] - m*rside[1]) + (lside[0] - m*lside[1])) / 2
        med = (.5-b)/m

        plt.plot([med], [50], color=colors[data[i].index(pt)], marker='o')
        plt.annotate(int(med), (med+(1.0/16)*med,50+data[i].index(pt)%2*4+1), color=colors[data[i].index(pt)])
    plt.legend(lines, legends,
               'lower right')
    ax.axis([xmin[i], xmax[i], 0, 100])
    plt.savefig(outputs[i])
