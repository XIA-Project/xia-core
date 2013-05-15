#!/usr/bin/python
# Filename: HeartbeatClient.py

"""Heartbeat client, sends out an UDP packet periodically"""

import socket, time, commands

SERVER_IP = socket.gethostbyname('GS11698.SP.CS.CMU.EDU')
SERVER_PORT = 43278
BEAT_PERIOD = 3
my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
my_color = 'red'
message = 'PyHB:%s %s' % (my_ip, my_color)

print ('Sending heartbeat to IP %s , port %d\n'
    'press Ctrl-C to stop\n') % (SERVER_IP, SERVER_PORT)
while True:
    hbSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    hbSocket.sendto(message, (SERVER_IP, SERVER_PORT))
    if __debug__: print 'Time: %s' % time.ctime()
    time.sleep(BEAT_PERIOD)
