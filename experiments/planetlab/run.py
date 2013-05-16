#!/usr/bin/python

cmd = 'echo test'
#cmd = './fedora-bin/xia-core/experiments/planetlab/test_infrastructure.py ./fedora-bin/xia-core/experiments/planetlab/tunneling.ini'

machines = file.open('machines','r').read().split('\n')
for machine in machines:
    c = 'ssh cmu_xia@%s %s' (machine, cmd)
    print c
    shlex.split(c)
