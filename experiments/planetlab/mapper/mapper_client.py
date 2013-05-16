#!/usr/bin/python

"""Heartbeat client, sends out an UDP packet periodically"""

import socket, time, commands, sys, shlex, re
from subprocess import Popen, PIPE

XROUTE = '/home/cmu_xia/fedora-bin/xia-core/bin/xroute -v'
SERVER_IP = socket.gethostbyname('GS11698.SP.CS.CMU.EDU')
SERVER_PORT = 43278
BEAT_PERIOD = 3
my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
myHID = ''
BROADCAST_HID = 'ffffffffffffffffffffffffffffffffffffffff'

if len(sys.argv) < 2:
    print "Usage: %s [color]" % sys.argv[0]
    sys.exit(-1)

my_color = sys.argv[1]

print ('Sending heartbeat to IP %s , port %d\n'
    'press Ctrl-C to stop\n') % (SERVER_IP, SERVER_PORT)
while True:
    try:
        process = Popen(shlex.split(XROUTE), stdout=PIPE)
        xr_out = process.communicate()
        rc = process.wait()
        if rc == 0:
            myHID = re.search(r'HID:(.*) *-2 \(self\)', xr_out[0]).group(1).strip().lower()
            neighbors = []
            for xline in xr_out[0].split('\n'):
                try:
                    neighbors.append(re.split(' *',xline)[4].split(':')[1])
                except:
                    pass
            neighbors = list(set(neighbors))
            neighbors = [neighbor.lower() for neighbor in neighbors]
            if myHID in neighbors: neighbors.remove(myHID)
            if BROADCAST_HID in neighbors: neighbors.remove(BROADCAST_HID)

            message = 'PyHB:%s;%s;%s;%s' %(my_ip, my_color, myHID, neighbors)
            print message
            hbSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            hbSocket.sendto(message, (SERVER_IP, SERVER_PORT))
            if __debug__: print 'Sent packet'
        else:
            myHID = '';
        if __debug__: print 'Time: %s' % time.ctime()
    except:
        pass
    time.sleep(BEAT_PERIOD)
