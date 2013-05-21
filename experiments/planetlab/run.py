#!/usr/bin/python

import shlex, sys
from subprocess import Popen, PIPE
from os.path import splitext

if len(sys.argv) < 3:
    print 'usage %s [topo_file] [start|stop]' % (sys.argv[0])
    sys.exit(-1)

cmd_output = open(splitext(sys.argv[1])[0]+'.ini', 'w')
process = Popen(shlex.split('./generate_commands.py %s' % (sys.argv[1])), stdout=cmd_output)
rc = process.wait()

STOP = '"sudo killall sh; sudo ~/fedora-bin/xia-core/bin/xianet stop; sudo killall mapper_client.py; sudo killall local_server.py"'
START = '"curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/init.sh > ./init.sh && chmod 755 ./init.sh && ./init.sh && ./fedora-bin/xia-core/experiments/planetlab/test_infrastructure.py ./fedora-bin/xia-core/experiments/planetlab/tunneling.ini"'
LS = '"ls"'
RM = '"rm -rf ~/*; rm -rf ~/.*"'

if sys.argv[2] == 'start':
    cmd = START
elif sys.argv[2] == 'stop':
    cmd = STOP
elif sys.argv[2] == 'ls':
    cmd = LS
elif sys.argv[2] == 'rm':
    cmd = RM

machines = open('machines','r').read().split('\n')
for machine in machines:
    try:
        name = machine.split('#')[1].strip()
        machine = machine.split('#')[0].strip()
        f = open('/tmp/%s-log' % (name),'w')
        c = 'ssh -o StrictHostKeyChecking=no cmu_xia@%s %s' % (machine, cmd)
        print c
        process = Popen(shlex.split(c),stdout=f,stderr=f)
    except:
        pass
