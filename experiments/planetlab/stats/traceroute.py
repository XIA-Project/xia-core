#!/usr/bin/python

import sys, shlex, commands
from subprocess import Popen, PIPE

my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]

if len(sys.argv) < 2:
    print 'usage: %s [machine_name]' % (sys.argv[0])
    sys.exit(-1)

process = Popen(shlex.split('sudo traceroute -I -w 1 ' + sys.argv[1]), stdout=PIPE)
out = process.communicate()
rc = process.wait()

stats = [int(out[0].split("\n")[-2].strip().split(' ')[0]), out[0].split('\n')[0].split(' ')[2]]
if stats[0] == 30:
    stats[0] = -1

message = 'PyStat:%s;traceroute;%s' % (my_ip, tuple(stats))
print message
