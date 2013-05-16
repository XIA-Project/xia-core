#!/usr/bin/python

import sys, socket

if len(sys.argv) < 2:
    print 'usage: %s [topofile.topo]' % (sys.argv[0])
    sys.exit(-1)

hosts = open('./machines','r').read().split('\n')
hostd = dict((x.split('#')[1],(x.split('#')[0],socket.gethostbyname(x.split('#')[0].strip()))) for x in hosts)

cmd = '[default]\n~/fedora-bin/xia-core/experiments/planetlab/mapper/mapper_client.py red&\n'

machines = open(sys.argv[1],'r').read().split('\n')
links = []
for machine in machines:
    name = machine.split(':')[0]
    neighbors = machine.split(':')[1].replace(' ','').split(',')
    links += [tuple(sorted((name,neighbor))) for neighbor in neighbors]    
links = list(set(links))
print links

for machine in machines:
    name = machine.split(':')[0]
    ip = hostd[name][1]
    host = hostd[name][0]
    line = '[%s] #%s\nuntil sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P ' % (ip, host)    

    neighbors = ['%s:400%s' % (hostd[''.join([n for n in link if n != name])][1],links.index(link)) for link in links if name in link]    
    line += ','.join(neighbors)
    line += ' -f eth0 start; do echo "restarting click"; done\n'
    cmd += line
    
print cmd
