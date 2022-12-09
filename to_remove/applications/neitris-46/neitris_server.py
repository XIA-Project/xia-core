#!/usr/bin/python
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
import sys
import os
                                                                                                                                                                                                                    
# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

import struct
import time, pygame
#import socket
import fcntl
from select import select
#from socket import socket, AF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR
from c_xsocket import *
import neitris_utils
import neitris_cfg

def now():
    return time.strftime("%I:%M:%S%p",time.localtime())

# Server Constants
HOSTNAME = "" # "" == localhost
#PORTS = [7777] #,7778]
SIDS = ['SID:1111111111222222222233333333334444444444']

# Make main sockets for accepting new client requests.
mainsocks, readsocks, writesocks = [],[],[]

# Create Reading Sockets:
for SID in SIDS:
    print 'Opening socket on %s'% SID
    portsock = Xsocket(XSOCK_STREAM)

    # Make a DAG to listen on 
    (myAD, myHID, my4ID) = XreadLocalHostAddr(portsock) 
    listen_dag = "DAG 3 0 1 - \n %s 3 2 - \n %s 3 0 - \n %s 3 - \n %s" % (myAD, my4ID, myHID, SID)
    listen_dag_re = "RE ( %s ) %s %s %s" % (my4ID, myAD, myHID, SID) 
    Xbind(portsock, listen_dag_re)
    print 'Listening on %s' % listen_dag

    # Publish DAG to naming service
    XregisterName("www_s.neitris.com.xia", listen_dag)
    print 'registered name'
    
    mainsocks.append(portsock)
    readsocks.append(portsock)

clients = {}

inbuf = {}
outbuf = {}


def ServeIt():
    global clients
    TICKEVENT = pygame.USEREVENT
    SPEEDEVENT = pygame.USEREVENT + 1
    
    SrvState = "Idle"    
    # Starting Select Server:
    done = 0
    while not done:


        if SrvState == "Starting":
            events = pygame.event.get()
            for event in events:
                if event.type == TICKEVENT:
                    print " got tickevent"
                    countdown = countdown - 1
                    msgdata = struct.pack("B", countdown)

                    for cli in clients:
                        msgout = neitris_utils.MsgPack(neitris_utils.GAMESTART,
                                                  msgdata, clients[cli][0], 0)                    
                        outbuf[cli].append(msgout)
                        if countdown > 0:
                            print "Counting: %d..." % (countdown)
                        else:
                            print "GO!!!"
                            pygame.time.set_timer(TICKEVENT, 0)
                            SrvState = "Playing"
                            pygame.time.set_timer(SPEEDEVENT,
                                                  neitris_cfg.SPEED_INCR_TIME)
                            
                            
        if SrvState == "Playing":
            events = pygame.event.get()
            for event in events:
                if event.type == SPEEDEVENT:
                    print " got speedevent"
                    #msgdata = struct.pack("B", INCRSPEED)
                    
                    for cli in clients:
                        msgout = neitris_utils.MsgPack(neitris_utils.INCRSPEED,
                                                  "", clients[cli][0], 0)  
                        outbuf[cli].append(msgout)

        # The one and only call to select - There is one missing optional argument
        # in the select call which can be used to set a timeout or timeout behavior.

        readables, writeables, exceptions = select(readsocks,readsocks,[]) #writesocks,[]) 

        for sockobj in readables:
            if sockobj in mainsocks:
                # port socket: accept new client
                #newsock, address = sockobj.accept()
                newsock = Xaccept(sockobj)
                address = '???'
                print 'Connect:', address, id(newsock)
                readsocks.append(newsock)
            else:
                # This is already an open connection, handle it
                try:
                    data = Xrecv(sockobj, 1024, 0)
                    #data = sockobj.recv(1024)

                    #print '\tgot', data, 'on', id(sockobj)

                    # No data received: the client has closed the connection
                    if not data:
                        #sockobj.close()
                        Xclose(sockobj)
                        readsocks.remove(sockobj)

                        
                        if SrvState == "Playing":
                            msgout = neitris_utils.MsgPack(neitris_utils.GAMEOVER,
                                                          "",0,
                                                          clients[id(sockobj)][0])
                            # Mark Client as Disconnected,
                            # to erase after gameover
                            for cli in clients:
                                if cli != id(sockobj):
                                    outbuf[cli].append(msgout)
                        
                        del clients[id(sockobj)]
                        del outbuf[id(sockobj)]
                        del inbuf[id(sockobj)]
                            
                        print "closing ", id(sockobj)
                        if len(clients) == 0:
                            print "Game Has ended due to client disconnection"
                            SrvState = "Idle"
                    # some data was received - process it
                    else:
                        try:
                            inbuf[id(sockobj)] = inbuf[id(sockobj)]  + data
                        except KeyError:
                            inbuf[id(sockobj)] =  data
                            
                        while 1:                       

                            # Wait for a full header 
                            if len(inbuf[id(sockobj)]) < 5:
                                break;
                            
                            (length, pload, cmd, dst, src) = \
                                     neitris_utils.MsgUnpack(inbuf[id(sockobj)])

                            # After reading the full header wait for a full
                            # packet, since we know now the packet length
                            if len(inbuf[id(sockobj)]) < length + 5:
                                break;
                            
                            # take the msg pload

                            msg = inbuf[id(sockobj)][5:(length+5)]
                            inbuf[id(sockobj)] = inbuf[id(sockobj)][(length+5):]

                            
                            # Now process the packet based on the cmd and the
                            # server's state                            
                            if cmd == neitris_utils.REGPLAYER:
                                if SrvState == "Idle":
                                    # find a PID to assign to new player
                                    for reg in range(1, 256):
                                        found = 1
                                        for cli in clients:
                                            if reg == clients[cli][0]:
                                                found = 0
                                                break;
                                        if found:
                                            pid = reg
                                            break

                                    if not found:
                                        continue
                                    
                                    #                     pid  msg St 
                                    clients[id(sockobj)]=[pid, msg, 0]
                                    
                                    pidpacked = struct.pack("B",pid)
                                    msgout = neitris_utils.MsgPack\
                                             (neitris_utils.REGPLAYERACK, pidpacked,pid,0)
                                    outbuf[id(sockobj)]=[]
                                    outbuf[id(sockobj)].append(msgout)
                                    print "New player joined the game..."
                                    print "     Player Name: ", msg
                                    print "              ID: ", pid
                                    print "       object ID: ", id(sockobj)
                                else:
                                    print "Cannot Join While game in progress"
                                    msgout = neitris_utils.MsgPack(neitris_utils.REGPLAYERNACK,
                                                                  "Game in progress",0,0)
                                    outbuf[id(sockobj)]=[]
                                    outbuf[id(sockobj)].append(msgout)
                                    readsocks.remove(sockobj)
                            elif cmd == neitris_utils.STARTREQ and SrvState == "Idle":
                                print "got STARTREQ MSG FROM ", id(sockobj)

                                clients[id(sockobj)][2] = 1
                                start = 1
                                for i in clients:
                                    if clients[i][2] == 0:
                                        start = 0
                                        break

                                if start == 1:
                                   
                                    pygame.time.set_timer(TICKEVENT, 1000)
                                    SrvState = "Starting"
                                    countdown = 4
                                    print "Server initiating Game Start"
                                    msgdata = struct.pack("B", len(clients))
                                    for cli in clients:
                                        msgdata = msgdata + struct.pack("!BH",
                                                                        clients[cli][0], len(clients[cli][1]))
                                        msgdata = msgdata + clients[cli][1]
                                        
                                        msgout = neitris_utils.MsgPack(neitris_utils.GAMEINFO, msgdata, 0, 0)

                                        print "queeued GAMEINFO for ", cli
                                    for cli in clients:
                                        outbuf[cli].append(msgout)
                                        start = 0
                            elif cmd == neitris_utils.SENDSTATE: # and \
#                                     SrvState == "Playing":
                                
                                msgout = neitris_utils.MsgPack(cmd,
                                                              pload[:length],
                                                              dst, src)
                                for cli in clients:
                                    if cli!= id(sockobj):
                                        outbuf[cli].append(msgout)

                            elif cmd == neitris_utils.GAMEOVER and \
                                 SrvState == "Playing":
                                print "Received Game over from ", id(sockobj)
                                msgout = neitris_utils.MsgPack(cmd, "", 0, src)
                                for cli in clients:
                                    if cli!= id(sockobj):
                                        outbuf[cli].append(msgout)

                                clients[id(sockobj)][2] = 0
                                end = 1
                                for i in clients:
                                    if clients[i][2] == 1:
                                        end = 0
                                        break

                                if end == 1:
                                    print " Game has ended..."
                                    pygame.time.set_timer(SPEEDEVENT, 0)
                                    msgout = neitris_utils.MsgPack(cmd,"",0,0)
                                    for cli in clients:
                                        outbuf[cli].append(msgout)

                                    # Send IncrVicts to winner
                                    msgout = neitris_utils.MsgPack(
                                        neitris_utils.INCRVICTS,"",
                                        clients[id(sockobj)][0],0)
                                    outbuf[id(sockobj)].append(msgout)
                                    SrvState = "Idle"
                            elif cmd == neitris_utils.POWERUP and \
                                 SrvState == "Playing":
                                msgout = neitris_utils.MsgPack(cmd,
                                                              pload[:length],
                                                              dst, src)
                                #print "powerup rcvd: ", pload[:length]
                                for cli in clients:
                                    if clients[cli][0] == dst:
                                        outbuf[cli].append(msgout)
                                
                                    
                                                

                     
                except:
                    raise
        # Get ready to write
       

        for sockobj in writeables:
            try:

                if outbuf[id(sockobj)] != []:
                    message = outbuf[id(sockobj)].pop(0)
                    #sockobj.send( "%s" % (message))
                    Xsend(sockobj, "%s" % (message), 0)
                    #print "sending to ", id(sockobj)," data: ", message
            except KeyError:
                pass


        pygame.time.wait(1)   

if __name__=="__main__":
    pygame.init()
    try:
        ServeIt()
    finally:
        for sock in mainsocks:
            #sock.close()
            Xclose(sock)
