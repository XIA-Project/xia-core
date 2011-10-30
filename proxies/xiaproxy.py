import socket, select
import struct, time, signal, os, sys
import fcntl
import xsocket
from ctypes import *

# Pretend a magic naming service givs us a dag for netloc...
from xia_address import *

def send_to_browser(data, browser_socket):
    try:
        browser_socket.send(data)
        return True
    except:
        print 'ERROR: xiaproxy.py: send_to_browser: error sending data to browser'
        browser_socket.close()
        return False

def recv_with_timeout(sock, timeout=5):
    # Make socket non-blocking
    try:
        fcntl.fcntl(sock, fcntl.F_SETFL, os.O_NONBLOCK)
    except IOError:
        print "ERROR: xiaproxy.py: recv_with_timeout: could not make socket nonblocking"
    
    # Receive data
    start_time = time.time()   # current time in seconds since the epoch
    received_data = False
    reply = '<html><head><title>XIA Error</title></head><body><p>&nbsp;</p><p>&nbsp;</p><p style="text-align: center; font-family: Tahoma, Geneva, sans-serif; font-size: xx-large; color: #666;">Sorry, something went wrong.</p><p>&nbsp;</p><p style="text-align: center; color: #999; font-family: Tahoma, Geneva, sans-serif;"><a href="mailto:xia-dev@cs.cmu.edu">Report a bug</a></p></body></html>'
    try:
        while (time.time() - start_time < timeout and not received_data):
            try:
                reply = xsocket.Xrecv(sock, 65521, 0)
                received_data = True
            except IOError:
                received_data = False
    except (KeyboardInterrupt, SystemExit), e:
        xsocket.Xclose(sock)
        sys.exit()
    return reply

def check_for_and_process_CIDs(message, browser_socket):
    rt = message.find('CID') 
    print rt
    if (rt!= -1):
        http_header = message[0:rt]
        content = requestCID(message[rt+4:rt+44])
	## this was the first occurrence
	send_to_browser(http_header, browser_socket)
	send_to_browser(content, browser_socket)
	## now keep finding CID and sending content
	#print "length received ",len(message)
	rt = message.find('CID', rt+44)
	print rt
	while(rt != -1):
		print "requesting for CID", message[rt+4:rt+44]
		content = requestCID(message[rt+4:rt+44])
		send_to_browser(content, browser_socket)
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
	send_to_browser(http_header, browser_socket)
	send_to_browser(content, browser_socket)
	## now keep finding CID and sending content
	#print "length received ",len(message)
	rt = message.find('CID', rt+44)
	while(rt != -1):
		print "requesting for CID", message[rt+4:rt+44]
		content = requestVideoCID(message[rt+4:rt+44], True)
		send_to_browser(content, browser_socket)
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
        content = recv_with_timeout(socks[i]) # = xsocket.Xrecv(socks[i], 1024, 0)
	send_to_browser(content, browser_socket)
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
        content = recv_with_timeout(socks[i]) # = xsocket.Xrecv(socks[i], 1024, 0)
        if not send_to_browser(content, browser_socket):
            return False        
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
    reply = recv_with_timeout(sock) # = xsocket.Xrecv(sock, 65521, 0)
    xsocket.Xclose(sock)
    numchunks = int(reply)
    print "sendSIDRequest: received reply for number of chunks ",numchunks

    ## return ogg header
    http_header = "HTTP/1.0 200 OK\r\nDate: Tue, 01 Mar 2011 06:14:58 GMT\r\nConnection: close\r\nContent-type: video/ogg\r\nServer: lighttpd/1.4.26\r\n\r\n"

    send_to_browser(http_header, browser_socket)

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
	reply = recv_with_timeout(sock) # = xsocket.Xrecv(sock, 1024, 0)
	xsocket.Xclose(sock)
	#print reply
	ret = process_videoCIDlist(reply, browser_socket, socks)
	if (ret==False):
	    break;
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
    data = recv_with_timeout(sock) # = xsocket.Xrecv(sock, 65521, 0)
    #print 'Retrieved content:\n%s' % data
    xsocket.Xclose(sock)
    return data



def sendSIDRequest(ddag, payload, browser_socket):
    # Create socket
    sock = xsocket.Xsocket()
    if (sock<0):
        print "ERROR: xiaproxy.py: sendSIDRequest: could not open socket"
        return



    #ddag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD1, IP1, HID1, sid)
    #ddag = "DAG 0 - \n %s 1 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP1, HID1, SID1)
    sdag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP0, HID0, SID0)    
    #sdag = "DAG 0 - \n %s 1 - \n %s 2 - \n %s 3 - \n %s" % (AD1, IP0, HID0, SID0)

    xsocket.Xbind(sock, sdag)

    rtt = time.time() 
    # Connect to service
    xsocket.Xconnect(sock, ddag)
    # Send request
    xsocket.Xsend(sock, payload, len(payload), 0)

    # Receive reply and close socket
    reply = recv_with_timeout(sock) # Use default timeout
    xsocket.Xclose(sock)

    # Pass reply up to browswer if it's a normal HTTP message
    # Otherwise request the CIDs
    contains_CIDs = check_for_and_process_CIDs(reply, browser_socket)
    if not contains_CIDs:
	rtt = int((time.time()-rtt) *1000)
	# Use last modified field to embedd RTT info
        reply = reply.replace("\n\n", ("\nLast-Modified:%d\n\n" % rtt)) 
        send_to_browser(reply, browser_socket)
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

    # Get content and close socket
    data = recv_with_timeout(sock) # Use default timeout
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
        send_to_browser(recombined_content, browser_socket)
    return

