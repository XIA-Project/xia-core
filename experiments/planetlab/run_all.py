#!/usr/bin/python

import shlex, sys
from subprocess import Popen, PIPE
from os.path import splitext

if len(sys.argv) < 3:
    print 'usage %s [all_nodes] [init|uname]' % (sys.argv[0])
    sys.exit(-1)

nodes = open(sys.argv[1],'r').read().split('\n')
nodes = [node.split('#')[0].strip() for node in nodes]

INIT = '"curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/init.sh > ./init.sh && chmod 755 ./init.sh && ./init.sh"'
UNAME = '"uname -r"'

if sys.argv[2] == 'init':
    cmd = INIT
elif sys.argv[2] == 'uname':
    cmd = UNAME

total = len(nodes)
current = 1

for node in nodes:
    try:
        c = 'ssh -o StrictHostKeyChecking=no cmu_xia@%s %s' % (node, cmd)
        sys.stdout.write('(%s/%s) %s: ' % (current, total, node))
        sys.stdout.flush()
        process = Popen(shlex.split(c),stdout=PIPE,stderr=PIPE)
        out = process.communicate()
        sys.stdout.write('\r(%s/%s) %s: %s' % (current, total, node, out[0]))
        sys.stdout.flush()
        rc = process.wait()
        current+=1;
    except:
        pass
