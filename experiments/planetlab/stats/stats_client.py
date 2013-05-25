#!/usr/bin/python

import sys, rpyc, socket, time
from check_output import check_output
from os.path import splitext

def try_until(f, args, msg):
    while True:
        try:
            x = f(*args)
        except:
            print msg
            time.sleep(1)
        else:
            return x

if len(sys.argv) < 4:
    print 'usage: %s [topofile.topo] [machines] [neighbor]' % (sys.argv[0])
    sys.exit(-1)

SERVER_NAME = 'GS11698.SP.CS.CMU.EDU'
SERVER_PORT = 43278
RPC_PORT = 5691
CLIENT_PORT = 3000
dir = '/home/cmu_xia/fedora-bin/xia-core/experiments/planetlab/stats/'
my_hostname = check_output('hostname')
neighbor = sys.argv[3]

out = check_output(dir + 'ping.py %s %s' % (sys.argv[1], sys.argv[2])).split(";")[2].split(")")[0]
ping = out.split("'")[1]
host = out.split("'")[3]
print ping, host

out = check_output(dir + 'traceroute.py %s' % (host))
hops = out.split(';')[-1].split('(')[1].split(',')[0]
print hops

master = rpyc.connect(SERVER_NAME,SERVER_PORT)
master.root.stats(my_hostname, neighbor, host, ping, hops)   
if __debug__: print 'Sent stats'

cmd_file = splitext(sys.argv[1])[0] + '.ini'
client = try_until(rpyc.connect, (host, RPC_PORT), '')
try_until(client.root.get_hid, (), '')
client.root.restart(cmd_file, socket.gethostbyname(my_hostname))

if __debug__: print 'Sent reconfigure packet'

cmd = 'until sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P %s:%s -f eth0 start; do echo "restarting click"; done' % (socket.gethostbyname(host), CLIENT_PORT)
print cmd
check_output(cmd)

nc = try_until(rpyc.connect, (neighbor, RPC_PORT), 'waiting for neighbor: %s' % neighbor)
nhid = try_until(nc.root.get_hid, (), 'waiting for neighbor xianet: %s' % neighbor)
nad = nc.root.get_ad()

print nad, nhid

out = check_output(dir + 'xping.py %s %s' % (nad, nhid))
xping = out.split(';')[-1].split("'")[1]

out = check_output(dir + 'xtraceroute.py %s %s' % (nad, nhid))
xhops = out.split(';')[-1].split('(')[1].split(',')[0]

print xping, xhops

master = rpyc.connect(SERVER_NAME,SERVER_PORT)
master.root.xstats(my_hostname, neighbor, xping, xhops)   
if __debug__: print 'Sent xstats'

