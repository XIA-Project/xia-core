#!/usr/bin/python

"""Heartbeat client, sends out an UDP packet periodically"""

import rpyc, time, commands, sys, re

XROUTE = '/home/cmu_xia/fedora-bin/xia-core/bin/xroute -v'
SERVER_NAME = 'GS11698.SP.CS.CMU.EDU'
SERVER_PORT = 43278
BEAT_PERIOD = 3
my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
myHID = ''
BROADCAST_HID = 'ffffffffffffffffffffffffffffffffffffffff'

if len(sys.argv) < 2:
    print "Usage: %s [color]" % sys.argv[0]
    sys.exit(-1)

my_color = sys.argv[1]
master = rpyc.connect(SERVER_NAME, SERVER_PORT)

print ('Sending heartbeat to IP %s , port %d\n'
    'press Ctrl-C to stop\n') % (SERVER_NAME, SERVER_PORT)
while True:
    try:
        xr_out = commands.getoutput(XROUTE)
        myHID = re.search(r'HID:(.*) *-2 \(self\)', xr_out).group(1).strip().lower()
        neighbors = []
        for xline in xr_out.split('\n'):
            try:
                neighbors.append(re.split(' *',xline)[4].split(':')[1])
            except:
                pass
        neighbors = list(set(neighbors))
        neighbors = [neighbor.lower() for neighbor in neighbors]
        if myHID in neighbors: neighbors.remove(myHID)
        if BROADCAST_HID in neighbors: neighbors.remove(BROADCAST_HID)

        try:
            master.root.heartbeat(my_ip, my_color, myHID, neighbors)
        except:
            master = rpyc.connect(SERVER_NAME, SERVER_PORT)
        print my_ip, my_color, myHID, neighbors
        if __debug__: print 'Sent packet'
        if __debug__: print 'Time: %s' % time.ctime()
    except Exception, e:
        print e
        pass
    time.sleep(BEAT_PERIOD)
