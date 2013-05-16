#!/usr/bin/python

import sys, socket

if len(sys.argv) < 2:
    print 'usage: %s [topofile.topo]' % (sys.argv[0])
    sys.exit(-1)

hosts = open('./machines','r').read().split('\n')
hostd = dict((x.split('#')[1],(x.split('#')[0],socket.gethostbyname(x.split('#')[0].strip()))) for x in hosts)

cmd = '[default]\n~/fedora-bin/xia-core/experiments/planetlab/mapper/mapper_client.py red&\n'

machines = open(sys.argv[1],'r').read().split('\n')
for machine in machines:
    name = machine.split(':')[0]
    neighbors = machine.split(':')[1].replace(' ','').split(',')
    ip = hostd[name][1]
    host = hostd[name][0]
    neighbors = [hostd[neighbor][1] for neighbor in neighbors]
    line = '[%s] #%s\nuntil sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P ' % (ip, host)    
    neighbors = ['%s:400%s' %(neighbor, neighbors.index(neighbor)) for neighbor in neighbors]
    line += ','.join(neighbors)
    line += ' -f eth0 start; do echo "restarting click"; done\n'
    cmd += line
    
print cmd
