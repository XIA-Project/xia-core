#!/usr/bin/python

import sys, shlex, commands
from subprocess import Popen, PIPE

my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]

if len(sys.argv) < 3:
    print 'usage: %s [target AD] [target HID]' % (sys.argv[0])
    sys.exit(-1)

traceroute= '/home/cmu_xia/fedora-bin/xia-core/bin/xtraceroute '
target = '"RE AD:%s HID:%s"' % (sys.argv[1], sys.argv[2])

process = Popen(shlex.split(traceroute + target), stdout=PIPE)
out = process.communicate()
rc = process.wait()

stats = [int(out[0].split('\n')[-2].split('=')[1].strip()), 'DAG']
if stats[0] == 30:
    stats[0] = -1

message = 'PyXStat:%s;xtraceroute;%s' % (my_ip, tuple(stats))
print message
