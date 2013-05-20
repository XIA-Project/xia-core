#!/usr/bin/python

import sys, shlex, commands
from subprocess import Popen, PIPE

my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
ping = 'traceroute ' 

if len(sys.argv) < 3:
    print 'usage: %s [topofile.topo] [machines]' % (sys.argv[0])
    sys.exit(-1)

hosts = open(sys.argv[2],'r').read().split('\n')
hostd = dict((x.split('#')[1],x.split('#')[0]) for x in hosts)

topo = open(sys.argv[1],'r').read()
backbones = topo.split('[backbone]')[1].split('[')[0].split('\n')[1:-1]
backbones = [backbone.strip() for backbone in backbones]

nodes = [hostd[backbone.split(':')[0]] for backbone in backbones]

processes = [Popen(shlex.split(ping + node), stdout=PIPE) for node in nodes]
outs = [process.communicate() for process in processes]
rcs = [process.wait() for process in processes]

stats = [(int(out[0].split("\n")[-2].split(' ')[0]), out[0].split('\n')[0].split(' ')[2]) for out in outs]
stats = sorted(stats)

message = 'PyStat:%s;traceroute;%s' % (my_ip, stats)
print message
