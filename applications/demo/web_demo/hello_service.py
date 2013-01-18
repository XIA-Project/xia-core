#!/usr/bin/python
import socket 
import c_xsocket
from xia_address import * 
import random
import sys
import os
from c_xsocket import *

set_conf("xsockconf_python.ini","hello_service.py")
print_conf()

#while(True):
try:
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
        ret= Xbind(sock,dag);
        print "listening on %s" % dag
        print "bind returns %d listening socket %d" % (ret, sock)
        
        while(True):
        	
        	accept_sock = Xaccept(sock);
        		
        	child_pid = os.fork()
  
  	  	if child_pid == 0:	
  	  	   #while(True):
    			replyto =  None
			dlen = None
        		#n = c_xsocket.Xrecvfrom(sock, 1500, 0, replyto, dlen)
        		
        		n = Xrecv(accept_sock, 1500, 0)
        		
        		hello_message = "<html><body><h1>Hello World!</h1></body></html>"
			http_header = "HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type:  text/html\n\n"
        		#c_xsocket.Xsendto(sock, stock_feed, len(stock_feed), 0, replyto, dlen)
			response = http_header+ hello_message
			#print "response len %d" % len(response)
			
        		Xsend(accept_sock, response, 0)
        		
        		Xclose(accept_sock)
        		os._exit(0)
except (KeyboardInterrupt, SystemExit), e:
       sys.exit()

c_xsocket.Xclose(sock)
