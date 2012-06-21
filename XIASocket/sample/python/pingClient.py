#import c_xsocket
import xsocket
import os
import sys
from ctypes import *

HID0= "HID:0000000000000000000000000000000000000000"
HID1= "HID:0000000000000000000000000000000000000001"
AD0=  "AD:1000000000000000000000000000000000000000"
AD1=  "AD:1000000000000000000000000000000000000001"
RHID0="HID:0000000000000000000000000000000000000002"
RHID1="HID:0000000000000000000000000000000000000003"
CID0= "CID:2000000000000000000000000000000000000001"
SID0= "SID:0f00000000000000000000000000000000000888"

#c_xsocket.set_conf("xsockconf_python.ini","pingClient.py")
#c_xsocket.print_conf()

#sock=c_xsocket.Xsocket(c_xsocket.XSOCK_STREAM)
s=xsocket.xsocket(xsocket.XSOCK_STREAM)

#if (sock<0):
#	print "error opening socket"
#	exit(-1)

dag = "RE %s %s %s" % (AD0, HID0, SID0)
#c_xsocket.Xconnect(sock,dag);  
try:
    s.connect(dag);  
except xsocket.error, msg:
    print msg

print 'connected'

print s.getsockopt(xsocket.XOPT_HLIM)
s.setsockopt(xsocket.XOPT_HLIM, 178)
print s.getsockopt(xsocket.XOPT_HLIM)

#payload ="hello world"
payload = "hello world"
#c_xsocket.Xsend(sock, payload,0)
s.send(payload)

#reply = c_xsocket.Xrecv(sock,1024,0)
reply = s.recv(1024)
print "payload: %s reply: %s %d" % (payload, reply, len(reply))

s.close()
