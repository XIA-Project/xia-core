#!/usr/bin/env python

import sys
import subprocess

print 'it will take some time...'

p = subprocess.Popen('bzcat rib.20110101.0000.bz2 | zebra-dump-parser/zebra-dump-parser.pl | sort -n | uniq', shell=True, stdout=subprocess.PIPE)

f_in = p.stdout
f_out = open('ip_routes.txt', 'w')

prev = None

later = ''

for l in f_in.readlines():
    cur = l.partition(' ')[0]
    if cur == prev:
        continue
    prev = cur

    if cur.startswith('0.0.0.0/'):
        later += '%s 0\n' % cur
    else:
        f_out.write('%s 0\n' % cur)

f_out.write(later)

p.wait()
f_out.close()

