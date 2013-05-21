#!/usr/bin/python

import sys, socket

if len(sys.argv) < 2:
    print 'usage: %s [topofile.topo]' % (sys.argv[0])
    sys.exit(-1)

hosts = open('./machines','r').read().split('\n')
hostd = dict((x.split('#')[1],(x.split('#')[0],socket.gethostbyname(x.split('#')[0].strip()))) for x in hosts)

cmd = '[default]\n~/fedora-bin/xia-core/experiments/planetlab/local_server.py&\n'

topo = open(sys.argv[1],'r').read()
backbones = topo.split('[backbone]')[1].split('[')[0].split('\n')[1:-1]
backbones = [backbone.strip() for backbone in backbones]

clients = topo.split('[clients]')[1].split('\n')[1:]
clients = [client.strip() for client in clients]

links = []
for machine in backbones:
    name = machine.split(':')[0]
    neighbors = machine.split(':')[1].replace(' ','').split(',')
    links += [tuple(sorted((name,neighbor))) for neighbor in neighbors]    
links = list(set(links))

for machine in backbones:
    name = machine.split(':')[0]
    ip = hostd[name][1]
    host = hostd[name][0]
    line = '[%s] #%s\n~/fedora-bin/xia-core/experiments/planetlab/mapper/mapper_client.py red&\nuntil sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P ' % (ip, host)    

    neighbors = ['%s:500%s' % (hostd[''.join([n for n in link if n != name])][1],links.index(link)) for link in links if name in link]    
    line += ','.join(neighbors)
    line += ' -f eth0 start; do echo "restarting click"; done\n'
    cmd += line
    
for machine in clients:
    name = machine
    ip = hostd[name][1]
    host = hostd[name][0]
    line = '[%s] #%s\nsleep 15\n~/fedora-bin/xia-core/experiments/planetlab/mapper/mapper_client.py blue&\n~/fedora-bin/xia-core/experiments/planetlab/stats/stats_client.py ~/fedora-bin/xia-core/experiments/planetlab/tunneling.topo ~/fedora-bin/xia-core/experiments/planetlab/machines\n' % (ip,host)
    cmd += line

print cmd
