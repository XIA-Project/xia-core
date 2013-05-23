#!/usr/bin/python

import sys, shlex, commands
from subprocess import Popen, PIPE

my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
interval = .25
count = 4
ping = '/home/cmu_xia/fedora-bin/xia-core/bin/xping -i %s -c ' % (interval)

if len(sys.argv) < 3:
    print 'usage: %s [target AD] [target HID]' % (sys.argv[0])
    sys.exit(-1)

target = '"RE AD:%s HID:%s"' % (sys.argv[1], sys.argv[2])

process = Popen(shlex.split(ping + '1 ' + target), stdout=PIPE)
out = process.communicate()
rc = process.wait()

process = Popen(shlex.split(ping + '%s ' % count + target), stdout=PIPE)
out = process.communicate()
rc = process.wait()

stat = (float(out[0].split("\n")[-2].split('=')[1].split('/')[1]), out[0].split('\n')[0].split(' ')[1])
stat = ("%.3f" % stat[0], stat[1])

message = 'PyXStat:%s;xping;%s' % (my_ip, stat)
print message
