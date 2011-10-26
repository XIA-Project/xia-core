import socket, select
import struct, time, signal, os, sys
import xsocket
from ctypes import *

# Pretend a magic naming service givs us a dag for netloc...
from xia_address import *

def check_for_and_process_CIDs(message, browser_socket):
    rt = message.find('CID') 
    print rt
    if (rt!= -1):
        http_header = message[0:rt]
        content = requestCID(message[rt+4:rt+44], True)
	## this was the first occurrence
	browser_socket.send(http_header)
	browser_socket.send(content)
	## now keep finding CID and sending content
	#print "length received ",len(message)
	rt = message.find('CID', rt+44)
	print rt
	while(rt != -1):
		print "requesting for CID", message[rt+4:rt+44]
		content = requestCID(message[rt+4:rt+44], True)
		browser_socket.send(content)
		rt = message.find('CID', rt+44)
	print "no more CID\n\n"
        return True
    print "NO CID present"
    return False


def check_for_and_process_CIDlist(message, browser_socket):
    rt = message.find('CID') 
    print rt
    if (rt!= -1):
        http_header = message[0:rt]
        content = requestVideoCID(message[rt+4:rt+44], True)
	## this was the first occurrence
	print header
	print content
	browser_socket.send(http_header)
	browser_socket.send(content)
	## now keep finding CID and sending content
	#print "length received ",len(message)
	rt = message.find('CID', rt+44)
	while(rt != -1):
		print "requesting for CID", message[rt+4:rt+44]
		content = requestVideoCID(message[rt+4:rt+44], True)
		browser_socket.send(content)
		rt = message.find('CID', rt+44)
        return True
    print "NO CID present"
    return False

def process_more_CIDlist(message, browser_socket, moresock, socks):
    rt = message.find('CID') 
    #print rt
    cidlist = list()
    while(rt != -1):
	#print "requesting for CID", message[rt+4:rt+44]
	CID = message[rt+4:rt+44]
	content_dag = 'CID:%s' % CID
        content_dag = 'RE %s %s %s' % (AD0, HID0, content_dag)
        cidlist.append(content_dag)
        #xsocket.XgetCID(moresock, content_dag, len(content_dag))
        #content = xsocket.Xrecv(moresock, 65521, 0)
	#browser_socket.send(content)
	rt = message.find('CID', rt+44)
    ## issue multiple request
    ## and receive multiple content
    ## first issue all the requests
    for i in range(len(cidlist)):
	xsocket.XgetCID(socks[i], cidlist[i], len(cidlist[i]))
    ## then retrieve them
    for i in range(len(cidlist)):
        content = xsocket.Xrecv(socks[i], 1024, 0)
	browser_socket.send(content)
    return True
 
def sendVideoSIDRequest(netloc, payload, browser_socket):
    print "in SID function - net location = ",netloc  

    sock = xsocket.Xsocket()
    if (sock<0):
        print "error opening socket"
        return
    dag = "RE %s %s %s" % (AD0, HID0, SID0) # Need a SID?
    print "Connecting to ",dag	
    xsocket.Xconnect(sock, dag)
    print "Connected. OK\n"

    # Send request
    xsocket.Xsend(sock, payload, len(payload), 0)
    # Receive reply
    print 'sendSIDRequest: about to receive reply'

    reply = xsocket.Xrecv(sock, 65521, 0)
    print "sendSIDRequest: received reply %s\n" % reply
    #,reply
    # Pass reply up to browswer if it's a normal HTTP message
    # Otherwise request the CIDs
    contains_CIDs = check_for_and_process_CIDlist(reply, browser_socket)
    if contains_CIDs:
	#see if it has more
	#print "Reached here\n"
    	found = reply.find('more')
	#print "foundvalue", found
	test = "hello world"
        moresock = xsocket.Xsocket()
	## assume at most 30 sockets
	socks = list()
	for i in range(30):
		sockcid = xsocket.Xsocket()
		socks.append(sockcid)
	while(found != -1):
		#if found
		## send data to server
		xsocket.Xsend(sock, test, len(test), 0)
		reply = xsocket.Xrecv(sock, 65521, 0)
		process_more_CIDlist(reply, browser_socket, moresock, socks)
		found = reply.find('more')
	xsocket.Xclose(moresock)
	for i in range(30):
		xsocket.Xclose(socks[i])
    else:
	browser_socket.send(reply)
    
    xsocket.Xclose(sock)
    return

def requestVideoCID(CID, fallback):
    sock = xsocket.Xsocket()
    if (sock<0):
        print "error opening socket"
        return
    # Request content
    content_dag = 'CID:%s' % CID
    if fallback:
        content_dag = 'RE %s %s %s' % (AD0, HID0, content_dag)
    #print 'Retrieving content with ID: \n%s' % content_dag
    xsocket.XgetCID(sock, content_dag, len(content_dag))
    # Get content
    data = xsocket.Xrecv(sock, 65521, 0)
    #print 'Retrieved content:\n%s' % data
    xsocket.Xclose(sock)
    return data



def sendSIDRequest(netloc, payload, browser_socket):
    sid = "SID:"+netloc[0:40]
    if (len(sid)!=len(SID1)):
        sid = SID1
    print "in SID function - net location = " + netloc + " sid: " + sid

    sock = xsocket.Xsocket()
    if (sock<0):
        print "error opening socket"
        return

    ddag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD1, IP1, HID1, sid)
    #ddag = "DAG 0 - \n %s 1 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP1, HID1, SID1)
    sdag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP0, HID0, SID0)    
    #sdag = "DAG 0 - \n %s 1 - \n %s 2 - \n %s 3 - \n %s" % (AD1, IP0, HID0, SID0)

    xsocket.Xbind(sock, sdag)

    # Connect to service
    xsocket.Xconnect(sock, ddag)
    # Send request
    xsocket.Xsend(sock, payload, len(payload), 0)

    # Receive reply
    try:
        print 'sendSIDRequest: about to receive reply'
        reply = xsocket.Xrecv(sock, 65521, 0)
        print "sendSIDRequest: received reply %s" % reply
    except (KeyboardInterrupt, SystemExit), e:
        sys.exit()
    finally:
        xsocket.Xclose(sock)
    # Pass reply up to browswer if it's a normal HTTP message
    # Otherwise request the CIDs
    contains_CIDs = check_for_and_process_CIDs(reply, browser_socket)
    if not contains_CIDs:
    	print("sending reply to browser")
        browser_socket.send(reply)
    
    return

def requestCID(CID, fallback):
    # TODO: fix issue where bare CIDs crash click
    print "in getCID function"  
    print CID

    sock = xsocket.Xsocket()
    if (sock<0):
        print "error opening socket"
        return

    # Request content
    content_dag = 'CID:%s' % CID    
    content_dag = "DAG 3 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD1, IP1, HID1, content_dag)
    sdag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP0, HID0, SID0)       
    #if fallback:
    #    content_dag = 'RE %s %s %s' % (AD1, HID1, content_dag)
    print 'Retrieving content with ID: \n%s' % content_dag
    xsocket.Xbind(sock, sdag);
    xsocket.XgetCID(sock, content_dag, len(content_dag))

    # Get content
    try:
        data = xsocket.Xrecv(sock, 65521, 0)
        print 'Retrieved content:\n len %d' % len(data)
    except (KeyboardInterrupt, SystemExit), e:
        sys.exit()
    finally:
        xsocket.Xclose(sock)

    return data


def xiaHandler(control, payload, browser_socket):
    xsocket.set_conf("xsockconf_python.ini", "xiaproxy.py")
    #xsocket.print_conf()  #for debugging

    if payload.find('GET /favicon.ico') != -1:
                    return
    print "in XIA code\n" + control + "\n" + payload
    control=control[4:]  # remove the 'xia.' prefix
    if control.find('sid') == 0:
        print "SID request"
        #print "%.6f" % time.time()
        if control.find('image.jpg') != -1: # TODO: why?
            payload = 'image.jpg'
        found = control.find('video');
        if(found != -1):
            sendVideoSIDRequest(control[4:], payload, browser_socket);
        else:
            sendSIDRequest(control[4:], payload, browser_socket);
    elif control.find('cid') == 0:
        print "CID request:\n%s" % control
        control_array = control.split('.')
        num_chunks = int(control_array[1])
        print "num chunks: %d" % num_chunks
        print 'CID list: %s' %control_array[2]
       
        # The browser might be requesting a list of chunks; if so, we'll recombine them into one object
        recombined_content = ''
        for i in range (0, num_chunks):
            print 'CID to fetch: %s' % control_array[2][i*40:40+i*40]
            recombined_content += requestCID(control_array[2][i*40:40+i*40], True)  #TODO: don't require fallback
            
        length = len (recombined_content)
        print "recombined_content length %d " % length
        browser_socket.send(recombined_content)
    return

