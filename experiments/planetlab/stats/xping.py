#!/usr/bin/python

import sys
from check_output import check_output

my_ip = check_output("/sbin/ifconfig").split("\n")[1].split()[1][5:]
interval = .25
count = 4
ping = '/home/cmu_xia/fedora-bin/xia-core/bin/xping -i %s -c ' % (interval)

if len(sys.argv) < 3:
    print 'usage: %s [target AD] [target HID]' % (sys.argv[0])
    sys.exit(-1)

target = '"RE AD:%s HID:%s"' % (sys.argv[1], sys.argv[2])

out = check_output(ping + '1 ' + target)
out = check_output(ping + '%s ' % count + target)

stat = (float(out[0].split("\n")[-2].split('=')[1].split('/')[1]), out[0].split('\n')[0].split(' ')[1])
stat = ("%.3f" % stat[0], stat[1])

message = 'PyXStat:%s;xping;%s' % (my_ip, stat)
print message
