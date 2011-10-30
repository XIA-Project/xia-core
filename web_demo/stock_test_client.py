#!/usr/bin/python
from xsocket import *
from xia_address import *

set_conf("xsockconf_python.ini","stock_test_client.py")
print_conf()

sock=Xsocket()

if (sock<0):
	print "error opening socket"
	exit(-1)

# Make the sDAG (the one the server listens on)
dag = "RE %s %s %s" % (AD1, HID1, SID_STOCK)
Xconnect(sock, dag)
msg = "hi"
#Xsendto(sock, msg ,len(msg),0, dag, len(dag)+1);
Xsend(sock, msg ,len(msg),0);

stock_feed = Xrecv(sock, 15000,0);
#stock_feed = Xrecvfrom(sock, 15000,0);

print stock_feed
print len(stock_feed)

