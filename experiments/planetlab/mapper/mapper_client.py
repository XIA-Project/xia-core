#!/usr/bin/python

"""Heartbeat client, sends out an UDP packet periodically"""

import socket, time, commands, sys

SERVER_IP = socket.gethostbyname('GS11698.SP.CS.CMU.EDU')
SERVER_PORT = 43278
BEAT_PERIOD = 3
my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]

if len(sys.argv) < 2:
    print "Usage: %s [color] [neighbor,neighbor,neighbor,...]" % sys.argv[0]
    sys.exit(-1)

my_color = sys.argv[1]
try:
    neighbors = sys.argv[2].split(",")
    ips = [socket.gethostbyname(x) for x in neighbors]
except:
    ips = []
message = 'PyHB:%s;%s;%s' % (my_ip, my_color, ips)
print message

print ('Sending heartbeat to IP %s , port %d\n'
    'press Ctrl-C to stop\n') % (SERVER_IP, SERVER_PORT)
while True:
    hbSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    hbSocket.sendto(message, (SERVER_IP, SERVER_PORT))
    if __debug__: print 'Time: %s' % time.ctime()
    time.sleep(BEAT_PERIOD)
