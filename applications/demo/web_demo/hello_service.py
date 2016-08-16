#!/usr/bin/python
import sys
import os
                                                                                                                                                                                                                    
# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

import socket 
import c_xsocket
from xia_address import * 
import random
from c_xsocket import *

#while(True):
try:
    SID_HELLO = XmakeNewSID()
    sock=Xsocket(XSOCK_STREAM)
        
    if (sock<0):
        print "error opening socket"
        exit(-1)

    # Get local AD and HID; build DAG to listen on
    (myAD, myHID, my4ID) = XreadLocalHostAddr(sock)
    # Make the sDAG (the one the server listens on)
    dag = "RE %s %s %s" % (myAD, myHID, SID_HELLO)  # TODO: Update dag to include 4ID
    # Publish DAG to naming service
    XregisterName("www_s.hello.com.xia", dag)
        
    # Bind to the DAG
    ret = Xbind(sock,dag)
    print "listening on %s" % dag
    print "bind returns %d listening socket %d" % (ret, sock)
        
    Xlisten(sock, 5)

    while(True):
        (accept_sock, peer) = Xaccept(sock)
        child_pid = os.fork()

        if child_pid == 0:
            n = Xrecv(accept_sock, 1500, 0)
            hello_message = "<html><body><h1>Hello World!</h1></body></html>"
                    
            content_length = len(hello_message)
            http_header = "HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type:  text/html\nContent-Length: %d\n\n" % content_length
            response = http_header + hello_message
                
            Xsend(accept_sock, response, 0)
        		
            Xclose(accept_sock)
            os._exit(0)
except (KeyboardInterrupt, SystemExit), e:
    c_xsocket.Xclose(sock)
    XremoveSID(SID_HELLO)
    sys.exit()
