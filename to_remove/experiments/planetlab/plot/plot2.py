#!/usr/bin/python

import sys
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

BIN_SIZE = 20
BGP_PROP_TIME = 1000
DNS_PROP_TIME = 5000
TUNNEL_SETUP_TIME = 2000

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



types = ['6RD', '4ID', 'SDN']
g = ['AG', 'BG']

if len(sys.argv) < 3:
    print 'usage: %s [stats-tunneling.txt] [stats-4id.txt]' % (sys.argv[0])
    sys.exit(-1)

pairs = open(sys.argv[2],'r').read().split('\n')[:-1]
pairs = [eval(pair.split('):')[1]) for pair in pairs]
for pair in pairs:
    for t in range(3):
        # pair: [6RD data, 4ID data, SDN data]
        # -->: {'type-AB': data, 'type-BA': data, 'type-AG': data, 'type-BG': data, 'type-GG': data}
        # -->-->: [source, dest, ping, hops]
        
        for i in range(2):
            try:
                ping = eval(pair[t]['%s-%s' % (types[t], g[i])][2])
                gg = eval(pair[t]['%s-%s' % (types[t], 'GG')][2])
                if ping != -1:
                    if t == 0: # 6rd
                        local[t+1].append(ping*2+BGP_PROP_TIME)
                    elif t == 1: # 4id
                        local[t+1].append(ping*2)
                    elif t == 2: # SDN
                        local[t+1].append(ping*2)
                if ping != -1 and gg != -1:
                    if t == 0: # 6rd
                        detection[t+1].append((gg+ping)*2)
                        remote[t+1].append(ping*2+DNS_PROP_TIME)
                    elif t == 1: # 4id
                        detection[t+1].append((gg+ping)*2)
                        remote[t+1].append((gg+ping)*2)
                    elif t == 2: # sdn
                        detection[t+1].append((gg+ping)*2)
                        remote[t+1].append((gg+ping)*2)
            except:
                pass


pairs = open(sys.argv[1],'r').read().split('\n')[:-1]
pairs = [eval(pair.split(':')[1]) for pair in pairs]
for pair in pairs:
    # pair: --> [BB, BB, Test, Test]
    # --> [(source, dest), 'backbone'/'test', ping, hops]        
    for i in range(2):
        try:
            ping = eval(pair[i][2])
            if ping != -1:
                local[0].append(ping*2+TUNNEL_SETUP_TIME)
        except:
            pass
        
        try:
            tohost = -1
            tohost = (eval(pair[3][2]) + eval(pair[2][2])) /2
            if tohost != -1:
                detection[0].append(tohost*2)
        except:
            pass
        
        try:
            if ping != -1 and tohost != -1:
                remote[0].append(ping*2+TUNNEL_SETUP_TIME+BGP_PROP_TIME)
        except:
            pass

for t in range(4):
    local[t] = sorted(local[t])
    detection[t] = sorted(detection[t])
    remote[t] = sorted(remote[t])

print len(local[0]), len(local[1]), len(local[2]), len(local[3])
print len(detection[0]), len(detection[1]), len(detection[2]), len(detection[3])
print len(remote[0]), len(remote[1]), len(remote[2]), len(remote[3])
# print local[0]
# print local[1]
# print local[2]
# print local[3]

data = [local, detection, remote]
filename = ['local', 'detection', 'remote']
titles = ['Recovery Time for Local Failures', 'Detection Time for Remote Failures', 'Recovery Time for Remote Failures']
for i in range(3):
    MAX = max(data[i][0][-1], data[i][1][-1], data[i][2][-1], data[i][3][-1]) + BIN_SIZE
    MAX = int(MAX - (MAX % BIN_SIZE))
    fig = plt.figure()
    plt.title('%s' % titles[i])
    ax = fig.add_subplot(111)
    ax.set_xlabel('Time (ms)')
    ax.set_ylabel('Percentage of Tests')
    X = [BIN_SIZE*x for x in range(0,MAX/BIN_SIZE)]
    for pt in data[i]:
        Y = {}
        for x in X:
            Y[x] = 0.0
        for l in pt:
            Y[l - (l % BIN_SIZE)] += (1.0/len(pt))

        p = sorted(zip(Y.keys(), Y.values()))
        plt.plot(zip(*p)[0], 100*np.cumsum(zip(*p)[1]))
    #plt.plot(zip(*p)[0], zip(*p)[1])
    plt.legend(('Tunneling', '6RD/6to4', '4ID', 'Smart 4ID'),
               'lower right')
    ax.axis([0, MAX, 0, 100])
    plt.savefig('/home/cmu_xia/%s.png' % filename[i])
