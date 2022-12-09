#!/usr/bin/python

import sys, thread


def std_listen(handle):
    while True:
        line = handle.readline().rstrip()
        if not line:
            return
        print line

def ssh_run(host, cmd):
    c = SSH_CMD+'%s %s' % (host, cmd)
    print '%s: launching subprocess: %s' % (host, cmd)
    p = Popen(SSH_CMD+'%s %s' % (host, cmd), shell=True, stdout=PIPE, stderr=PIPE)
    thread.start_new_thread(std_listen, (p.stdout,))
    thread.start_new_thread(std_listen, (p.stderr,))
    p.wait()
    print '%s: finished running subprocess: %s' % (host, cmd)

sys.path.insert(0, 'scripts')

from commands import getoutput
from subprocess import Popen, PIPE
from plcommon import check_output

nodes = getoutput('ls /tmp/ | grep log').split('\n')
nodes = [node.split('-log')[0].strip() for node in nodes]

SSH_CMD = 'ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 cmu_xia@'
STOP_CMD = '"sudo killall sh; sudo killall init.sh; sudo killall rsync; sudo ~/fedora-bin/xia-core/bin/xianet stop; sudo killall mapper_client.py; sudo killall local_server.py; sudo killall python; sudo killall xping; sudo killall xtraceroute"'

print len(nodes)

for node in nodes:
    try:
        ssh_run(node, STOP_CMD)
    except Exception, e:
        print e
        pass
