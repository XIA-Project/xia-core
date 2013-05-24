#!/usr/bin/python

import commands, sys, rpyc, socket, time
from subprocess import Popen, PIPE, call
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
my_hostname = commands.getoutput('hostname')
neighbor = sys.argv[3]

out = commands.getoutput(dir + 'ping.py %s %s' % (sys.argv[1], sys.argv[2])).split(";")[2].split(")")[0]
ping = out.split("'")[1]
host = out.split("'")[3]
print ping, host

out = commands.getoutput(dir + 'traceroute.py %s' % (host))
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
call(cmd, shell=True)

nc = try_until(rpyc.connect, (neighbor, RPC_PORT), 'waiting for neighbor: %s' % neighbor)
nhid = try_until(nc.root.get_hid, (), 'waiting for neighbor xianet: %s' % neighbor)
nad = nc.root.get_ad()

print nad, nhid

i = 0
while True:
    out = commands.getoutput(dir + 'xping.py %s %s' % (nad, nhid))
    print out
    xping = out.split(';')[-1].split("'")[1]
    i+=1
    if i is 5 or xping is not '=':
        break

i = 0
while True:
    out = commands.getoutput(dir + 'xtraceroute.py %s %s' % (nad, nhid))
    xhops = out.split(';')[-1].split('(')[1].split(',')[0]
    i+=1
    if i is 5 or xhops is not '-1':
        break

print xping, xhops

master = rpyc.connect(SERVER_NAME,SERVER_PORT)
master.root.xstats(my_hostname, neighbor, xping, xhops)   
if __debug__: print 'Sent xstats'

