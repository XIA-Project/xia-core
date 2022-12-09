#!/usr/bin/python

import sys, os
from os.path import dirname

if len(sys.argv) < 2:
   print 'usage %s [name]' % (sys.argv[0])
   sys.exit(-1)

machines = open(dirname(sys.argv[0])+'/names','r').read().split('\n')[:-1]
for machine in machines:
    if machine.split(' ')[1].split('#')[1] == sys.argv[1]:
       os.system('ssh cmu_xia@%s' % (machine.split(' ')[0]))
