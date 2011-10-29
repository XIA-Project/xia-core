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
        content = requestCID(message[rt+4:rt+44])
	## this was the first occurrence
	browser_socket.send(http_header)
	browser_socket.send(content)
	## now keep finding CID and sending content
	#print "length received ",len(message)
	rt = message.find('CID', rt+44)
	print rt
	while(rt != -1):
		print "requesting for CID", message[rt+4:rt+44]
		content = requestCID(message[rt+4:rt+44])
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
	print "Sending first chunk \n";
        content = requestVideoCID(message[rt+4:rt+44], True)
	## this was the first occurrence
	#print header
	#print content
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
        #content_dag = 'RE %s %s %s' % (AD1, HID1, content_dag)
        content_dag = 'DAG 2 0 - \n %s 2 1 - \n %s 2 - \n %s' % (AD1, HID1, content_dag)
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

def process_videoCIDlist(message, browser_socket, socks):
    rt = message.find('CID') 
    #print rt
    cidlist = list()
    while(rt != -1):
	#print "requesting for CID", message[rt+4:rt+44]
	CID = message[rt+4:rt+44]
	content_dag = 'CID:%s' % CID
        #content_dag = 'RE %s %s %s' % (AD1, HID1, content_dag)
        content_dag = 'DAG 2 0 - \n %s 2 1 - \n %s 2 - \n %s' % (AD1, HID1, content_dag)
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
    print "Debugging: in SID function - net location = ",netloc  

    sock = xsocket.Xsocket()
    if (sock<0):
        print "error opening socket"
        return
    dag = "RE %s %s %s" % (AD1, HID1, SID_VIDEO) # Need a SID?
    print "Connecting to ",dag	
    xsocket.Xconnect(sock, dag)
    print "Connected. OK\n"
    # Send request for number of chunks
    asknumchunks = "numchunks";
    xsocket.Xsend(sock, asknumchunks, len(asknumchunks), 0)
    #xsocket.Xsend(sock, payload, len(payload), 0)
    # Receive reply
    print 'sendSIDRequest: about to receive reply'
    reply = xsocket.Xrecv(sock, 65521, 0)
    xsocket.Xclose(sock)
    numchunks = int(reply)
    print "sendSIDRequest: received reply for number of chunks ",numchunks

    ## return ogg header
    http_header = "HTTP/1.0 200 OK\r\nDate: Tue, 01 Mar 2011 06:14:58 GMT\r\nConnection: close\r\nContent-type: video/ogg\r\nServer: lighttpd/1.4.26\r\n\r\n"

    browser_socket.send(http_header)

    ## next get chunks, at most 20 in a go
    threshold = 20
    socks = list()
    for i in range(threshold):
	sockcid = xsocket.Xsocket()
	socks.append(sockcid)
    num_iterations = (numchunks/threshold) + 1
    for i in range(num_iterations):
        st_cid = i * threshold
        end_cid = (i+1) * threshold
        if(end_cid > numchunks):
		end_cid = numchunks
        cidreqrange = str(st_cid) + ":" + str(end_cid)
	#print "Requesting for ",cidreqrange
        sock = xsocket.Xsocket()
        xsocket.Xconnect(sock, dag)
	xsocket.Xsend(sock, cidreqrange, len(cidreqrange), 0)
	reply = xsocket.Xrecv(sock, 1024, 0)
	xsocket.Xclose(sock)
	#print reply
	process_videoCIDlist(reply, browser_socket, socks)
	## process CIDs 
    for i in range(threshold):
	xsocket.Xclose(socks[i])
    return

def requestVideoCID(CID, fallback):
    sock = xsocket.Xsocket()
    if (sock<0):
        print "error opening socket"
        return
    # Request content
    content_dag = 'CID:%s' % CID
    if fallback:
        content_dag = 'RE %s %s %s' % (AD1, HID1, content_dag)
    #print 'Retrieving content with ID: \n%s' % content_dag
    xsocket.XgetCID(sock, content_dag, len(content_dag))
    # Get content
    data = xsocket.Xrecv(sock, 65521, 0)
    #print 'Retrieved content:\n%s' % data
    xsocket.Xclose(sock)
    return data



def sendSIDRequest(ddag, payload, browser_socket):
    #sid = "SID:"+netloc[0:40]
    #if (len(sid)!=len(SID1)):
    #    sid = SID1
    #print "in SID function - net location = " + netloc + " sid: " + sid

    sock = xsocket.Xsocket()
    if (sock<0):
        print "error opening socket"
        return

    #ddag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD1, IP1, HID1, sid)
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

def requestCID(CID):
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


def xiaHandler(control, path, payload, browser_socket):
    print "in XIA code\n" + control + "\n" + payload
    
    # Configure XSocket
    xsocket.set_conf("xsockconf_python.ini", "xiaproxy.py")
    #xsocket.print_conf()  #for debugging
    control=control[4:]  # remove the 'xia.' prefix


    if payload.find('GET /favicon.ico') != -1:
                    return
    if control.find('sid') == 0:
        print "SID request"
        if control.find('image.jpg') != -1: # TODO: why?
            payload = 'image.jpg'
        found = control.find('video');
        if(found != -1):
            sendVideoSIDRequest(control[4:], payload, browser_socket);
        else:
            # Do some URL processing 
            ddag = dag_from_url(control + path)
            sendSIDRequest(ddag, payload, browser_socket);
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
            recombined_content += requestCID(control_array[2][i*40:40+i*40])
            
        length = len (recombined_content)
        print "recombined_content length %d " % length
        browser_socket.send(recombined_content)
    return

