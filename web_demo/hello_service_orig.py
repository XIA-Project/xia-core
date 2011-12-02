#!/usr/bin/python
import xsocket 
from xia_address import * 
import random
import sys

xsocket.set_conf("xsockconf_python.ini","hello_service.py")
xsocket.print_conf()

while(True):
    try:
        sock=xsocket.Xsocket(0)
        
        if (sock<0):
        	print "error opening socket"
        	exit(-1)
        
        # Make the sDAG (the one the server listens on)
        dag = "RE %s %s %s" % (AD1, HID1, SID_HELLO)
        
        # Bind to the DAG
        ret= xsocket.Xbind(sock,dag);
        print "listening on %s" % dag
        print "bind returns %d socket %d" % (ret, sock)
        
        xsocket.Xaccept(sock);
    	replyto =  None
	dlen = None
        #n = xsocket.Xrecvfrom(sock, 1500, 0, replyto, dlen)
        n = xsocket.Xrecv(sock, 1500, 0)
        hello_message = "<html><body><h1>Hello World!</h1></body></html>"
	http_header = "HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type:  text/html\n\n"
        #xsocket.Xsendto(sock, stock_feed, len(stock_feed), 0, replyto, dlen)
	response = http_header+ hello_message
	print "response len %d" % len(response)
        xsocket.Xsend(sock, response, len(response), 0)
        xsocket.Xclose(sock)
    except (KeyboardInterrupt, SystemExit), e:
            sys.exit()

xsocket.Xclose(sock)
