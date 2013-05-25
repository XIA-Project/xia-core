#!/usr/bin/python

import sys
from check_output import check_output

my_ip = check_output("/sbin/ifconfig").split("\n")[1].split()[1][5:]
interval = .25
count = 4
ping = 'sudo ping -W 1 -i %s -c ' % (interval)

if len(sys.argv) < 3:
    print 'usage: %s [topofile.topo] [machines]' % (sys.argv[0])
    sys.exit(-1)

hosts = open(sys.argv[2],'r').read().split('\n')
hostd = dict((x.split('#')[1],x.split('#')[0]) for x in hosts)

topo = open(sys.argv[1],'r').read()
backbones = topo.split('[backbone]')[1].split('[')[0].split('\n')[1:-1]
backbones = [backbone.strip() for backbone in backbones]

nodes = [hostd[backbone.split(':')[0]] for backbone in backbones]

outs = [check_output(ping + '1 ' + node) for node in nodes]
outs = [check_output(ping + '%s ' % count + node) for node in nodes]
outs = zip(outs, rcs)

stats = []
for out in outs:
    host = out[0][0].split('\n')[0].split(' ')[1]
    p = 5000.00
    if out[1] == 0:
        p = float(out[0][0].split("\n")[-2].split('=')[1].split('/')[1])
    stats.append((p,host))
stats = sorted(stats)
stats = [(-1,stat[1]) if stat[0] == 5000 else stat for stat in stats]
stats = [("%.3f" % stat[0], stat[1]) for stat in stats]

message = 'PyStat:%s;ping;%s' % (my_ip, stats)
print message
