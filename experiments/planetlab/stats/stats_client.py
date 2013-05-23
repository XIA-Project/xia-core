#!/usr/bin/python

import commands, sys, re, rpyc, socket, time
from subprocess import Popen, PIPE, call
from os.path import splitext

if len(sys.argv) < 4:
    print 'usage: %s [topofile.topo] [machines] [neighbor]' % (sys.argv[0])
    sys.exit(-1)

SERVER_NAME = 'GS11698.SP.CS.CMU.EDU'
SERVER_PORT = 43278
RPC_PORT = 5691
CLIENT_PORT = 3000
dir = '/home/cmu_xia/fedora-bin/xia-core/experiments/planetlab/stats/'
my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]

out = commands.getoutput(dir + 'ping.py %s %s' % (sys.argv[1], sys.argv[2])).split(";")[2].split(")")[0]
ping = out.split("'")[1]
host = out.split("'")[3]
print ping, host

out = commands.getoutput(dir + 'traceroute.py %s' % (host))
hops = re.search(r"\((\d*), '%s'\)" % (host),out).group(1)
print hops

master = rpyc.connect(SERVER_NAME,SERVER_PORT)
master.root.stats(my_ip, host, ping, hops)   
if __debug__: print 'Sent stats'

cmd_file = splitext(sys.argv[1])[0] + '.ini'
while True:
    try:
        client = rpyc.connect(host, RPC_PORT)
    except:
        time.sleep(1)
    else:
        break
while True:
    try:
        client.root.get_hid()
    except:
        time.sleep(1)
    else:
        break
client.root.restart(cmd_file, my_ip)

if __debug__: print 'Sent reconfigure packet'

cmd = 'until sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P %s:%s -f eth0 start; do echo "restarting click"; done' % (socket.gethostbyname(host), CLIENT_PORT)
print cmd
call(cmd, shell=True)

neighbor = sys.argv[3]
while True:
    try:
        nc = rpyc.connect(neighbor, RPC_PORT)
    except:
        print 'waiting for neighbor: %s' % neighbor
        time.sleep(1)
    else:
        break
while True:
    try:
        nhid = nc.root.get_hid()
    except:
        print 'waiting for neighbor xianet: %s' % neighbor
        time.sleep(1)
    else:
        break
nad = nc.root.get_ad()
dir = '/home/cmu_xia/fedora-bin/xia-core/bin/'

out = commands.getoutput(dir + 'xping.py %s %s' % (nad, nhid))
xping = out.split(';')[-1].split("'")[1]

out = commands.getoutput(dir + 'xtraceroute.py %s %s' % (nad, nhid))
xhops = out.split(';')[-1].split('(')[1].split(',')[0]

print xping, xhops

master = rpyc.connect(SERVER_NAME,SERVER_PORT)
master.root.xstats(my_ip, neighbor, xping, xhops)   
if __debug__: print 'Sent xstats'

