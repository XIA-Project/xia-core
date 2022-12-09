#
#    Neitris - A multiplayer Tetris clone 
#    Copyright (C) 2006 Alexandros Kostopoulos (akostopou@gmail.com)
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
import time, sys
import struct

import select
from socket import socket, AF_INET, SOCK_STREAM
from c_xsocket import *

#import neitris


# These are server side calls
#sockobj.bind((HOSTNAME, PORT))
#sockobj.listen(5)

message = ["Ping!"]

connected = 0
#readsock = None

#------------ Threaded Stuff ----------------#
DONE = 0
# I'm using PORTS 7777 and 7778
def ChatRead(PORT, rqueue, readsock):
#    global readsock
    

    buf = ''
    while not DONE:
        #data = readsock.recv(1024)
        while select.select([readsock], [], []) == []:
            busywait = True
        data = Xrecv(readsock, 1024, 0)
        buf = buf + data

        while 1:
            if len(buf) >= 2:
                (lenbuf,) = struct.unpack("!H", buf[:2])
            else:
                break

            if len(buf) >= lenbuf + 2:
            #print "playername: ", buf[0]
#            if buf[2] != playername:
                rqueue.put_nowait(buf[:lenbuf+2])
                
                buf = buf[(lenbuf+2):]
                
            else:
                break

        
    #readsock.close()
    Xclose(readsock)
    return

# This modified version of ChatWrite aggregates messages and only calls
# Xsend once every 0.5 seconds
def ChatWrite(PORT, wqueue, readsock):
#    global readsock

    data = ''
    lastsend = 0  # keep track of when we send messages
    while not DONE:
   
        while not wqueue.empty():
            data += wqueue.get_nowait()

        # Make sure we don't send more often than 0.5 sec
        if (time.time() - lastsend < 0.5 or data == ''):
            continue

        Xsend(readsock, data, 0)
        lastsend = time.time()
        data = ''

        #time.sleep(0.2)
    #writesock.close()
    return

    
#def ChatWrite(PORT, wqueue, readsock):
##    global readsock
#
#    while not DONE:
#   
#        while not wqueue.empty():
#            data += wqueue.get_nowait()
#
#            #readsock.send(data)
#            Xsend(readsock, data, 0)
#            time.sleep(0.5)
#
#        #time.sleep(0.2)
#    #writesock.close()
#    return
