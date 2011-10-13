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
SID0= "SID:0f00000000000000000000000000000000000055"

xsocket.set_conf("xsockconf_python.ini","pingClient.py")
xsocket.print_conf()

sock=xsocket.Xsocket()

if (sock<0):
	print "error opening socket"
	exit(-1)

dag = "RE %s %s %s" % (AD0, HID0, SID0)
xsocket.Xconnect(sock,dag);  

#payload ="hello world"
payload = "hello world"
xsocket.Xsend(sock, payload, len(payload),0)
#xsocket.Xsend(sock, payload, 10,0)

reply = xsocket.Xrecv(sock,1024,0)
print "payload: %s reply: %s %d" % (payload, reply, len(reply))
