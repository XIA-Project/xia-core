import xsocket
HID0= "HID:0000000000000000000000000000000000000000"
HID1= "HID:0000000000000000000000000000000000000001"
AD0=  "AD:1000000000000000000000000000000000000000"
AD1=  "AD:1000000000000000000000000000000000000001"
RHID0="HID:0000000000000000000000000000000000000002"
RHID1="HID:0000000000000000000000000000000000000003"
CID0= "CID:2000000000000000000000000000000000000001"
SID0= "SID:0f00000000000000000000000000000000000055"
xsocket.set_conf("xsockconf_python.ini","CIDrequest.py")
xsocket.print_conf()
def main():
	sock=xsocket.Xsocket()
	if (sock<0):
		print "error opening socket"
		exit(-1)
	dag = "RE %s %s %s" % (AD0, HID0, SID0)
	xsocket.Xconnect(sock,dag);  
	cid = "CID:0000000000000000000000000000000000000000"
	print "trying ",cid
	dag = "RE %s %s %s" % (AD0, HID0, cid)
	xsocket.XgetCID(sock, dag, len(dag))
	reply = xsocket.Xrecv(sock, 8000, 0)
	print reply
	print "Received length ",len(reply)
main()
