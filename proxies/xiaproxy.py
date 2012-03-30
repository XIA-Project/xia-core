import socket, select, random
import struct, time, signal, os, sys, re
import fcntl
from xsocket import *
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
    ## Make socket non-blocking
    #try:
    #    fcntl.fcntl(sock, fcntl.F_SETFL, os.O_NONBLOCK)
    #except IOError:
    #    print "ERROR: xiaproxy.py: recv_with_timeout: could not make socket nonblocking"
    
    # Receive data
    start_time = time.time()   # current time in seconds since the epoch
    received_data = False
    reply = '<html><head><title>XIA Error</title></head><body><p>&nbsp;</p><p>&nbsp;</p><p style="text-align: center; font-family: Tahoma, Geneva, sans-serif; font-size: xx-large; color: #666;">Sorry, something went wrong.</p><p>&nbsp;</p><p style="text-align: center; color: #999; font-family: Tahoma, Geneva, sans-serif;"><a href="mailto:xia-dev@cs.cmu.edu">Report a bug</a></p></body></html>'
    try:
        while (time.time() - start_time < timeout and not received_data):
            try:
                select.select([sock], [], [], 0.02)
                reply = Xrecv(sock, 65521, 0)
                received_data = True
            except IOError:
                received_data = False
            except:
                print 'ERROR: xiaproxy.py: recv_with_timeout: error receiving data from socket'
    except (KeyboardInterrupt, SystemExit), e:
        Xclose(sock)
        sys.exit()

    if (not received_data):
        print "Recieved nothing"
        raise IOError

    return reply
    
    

def recvfrom_with_timeout(sock, timeout=3):
    # Make socket non-blocking
    try:
        fcntl.fcntl(sock, fcntl.F_SETFL, os.O_NONBLOCK)
    except IOError:
        print "ERROR: xiaproxy.py: recv_with_timeout: could not make socket nonblocking"
    
    # Receive data
    start_time = time.time()   # current time in seconds since the epoch
    received_data = False
    reply = '<html><head><title>XIA Error</title></head><body><p>&nbsp;</p><p>&nbsp;</p><p style="text-align: center; font-family: Tahoma, Geneva, sans-serif; font-size: xx-large; color: #666;">Sorry, something went wrong.</p><p>&nbsp;</p><p style="text-align: center; color: #999; font-family: Tahoma, Geneva, sans-serif;"><a href="mailto:xia-dev@cs.cmu.edu">Report a bug</a></p></body></html>'
    reply_dag = ''
    try:
        while (time.time() - start_time < timeout and not received_data):
            try:
                select.select([sock], [], [], 0.02)
                (reply, reply_dag) = Xrecvfrom(sock, 65521, 0)
                received_data = True
            except IOError:
                received_data = False
            except:
                print 'ERROR: xiaproxy.py: recvfrom_with_timeout: error receiving data from socket'
    except (KeyboardInterrupt, SystemExit), e:
        Xclose(sock)
        sys.exit()

    if (not received_data):
        print "Recieved nothing"
        raise IOError

    return reply, reply_dag    


def readcid_with_timeout(sock, cid, timeout=5):
    # Receive data
    start_time = time.time()   # current time in seconds since the epoch
    received_data = False
    reply = '<html><head><title>XIA Error</title></head><body><p>&nbsp;</p><p>&nbsp;</p><p style="text-align: center; font-family: Tahoma, Geneva, sans-serif; font-size: xx-large; color: #666;">Sorry, something went wrong.</p><p>&nbsp;</p><p style="text-align: center; color: #999; font-family: Tahoma, Geneva, sans-serif;"><a href="mailto:xia-dev@cs.cmu.edu">Report a bug</a></p></body></html>'
    try:
        while (time.time() - start_time < timeout and not received_data):
            try:
                if XgetChunkStatus(sock, cid, len(cid)) == 1:
                    reply = XreadChunk(sock, 65521, 0, cid, len(cid))
                    received_data = True
            except IOError:
                received_data = False
            except:
                print 'ERROR: xiaproxy.py: readcid_with_timeout: error receiving data from socket'
    except (KeyboardInterrupt, SystemExit), e:
        Xclose(sock)
        sys.exit()

    if (not received_data):
        print "Recieved nothing"
        raise IOError

    return reply
    
    

def check_for_and_process_CIDs(message, browser_socket):
    rt = message.find('cid')
    if (rt!= -1):
        http_header = message[0:rt]
        try:
            content = get_content_from_cid_list_temp(message[rt:].split('.')[2])
        except:
            print "ERROR: xiaproxy.py: check_for_and_process_CIDs: Couldn't retrieve content. Closing browser_socket"
            browser_socket.close()
            return True
        send_to_browser(http_header, browser_socket)
        send_to_browser(content, browser_socket)
        return True
    else:
        return False

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
        #XrequestChunk(moresock, content_dag, len(content_dag))
        #content = Xrecv(moresock, 65521, 0)
	#browser_socket.send(content)
	rt = message.find('CID', rt+44)
    ## issue multiple request
    ## and receive multiple content
    ## first issue all the requests
    for i in range(len(cidlist)):
        try:
            XrequestChunk(socks[i], cidlist[i], len(cidlist[i]))
        except:
            print 'ERROR: xiaproxy.py: process_videoCIDlist: error requesting CID %s' % cidlist[i]
    ## then retrieve them
    for i in range(len(cidlist)):
        try:
            content = readcid_with_timeout(socks[i], cidlist[i], 2)
	except:
	    browser_socket.close()
	    print "closing browser socket6"
	    return False
        if not send_to_browser(content, browser_socket):
            return False        
    return True


 
def sendVideoSIDRequest(netloc, payload, browser_socket):
    print "Debugging: in SID function - net location = ",netloc  

    sock = Xsocket(XSOCK_STREAM)
    if (sock<0):
        print "error opening socket"
        return
    dag = "RE %s %s %s" % (AD1, HID1, SID_VIDEO) # Need a SID?
  
    status = Xconnect(sock, dag)
    if (status != 0):
       	print "Unexpected error:", sys.exc_info()[0]
       	Xclose(sock)
    	print "sendSIDRequestXSP() Closing browser socket "
    	browser_socket.close()
	return
    
    print "Connected. OK\n"
    # Send request for number of chunks
    asknumchunks = "numchunks";
    Xsend(sock, asknumchunks, len(asknumchunks), 0)
    #Xsend(sock, payload, len(payload), 0)
    # Receive reply
    print 'sendSIDRequestXSP: about to receive reply'
    try:
        reply = recv_with_timeout(sock) # = Xrecv(sock, 65521, 0)
    except:
        Xclose(sock)
    	print "closing browser socket7"
	browser_socket.close()
	return

    Xclose(sock)
    numchunks = int(reply)
    print "sendSIDRequestXSP: received reply for number of chunks ",numchunks

    ## may be send http header along with first content
    ## return ogg header
    http_header = "HTTP/1.0 200 OK\r\nDate: Tue, 01 Mar 2011 06:14:58 GMT\r\nConnection: close\r\nContent-type: video/ogg\r\nServer: lighttpd/1.4.26\r\n\r\n"


    ## next get chunks, at most 20 in a go
    threshold = 20
    socks = list()
    for i in range(threshold):
	sockcid = Xsocket(XSOCK_CHUNK)
	socks.append(sockcid)
    num_iterations = (numchunks/threshold) + 1
    for i in range(num_iterations):
        st_cid = i * threshold
        end_cid = (i+1) * threshold
        if(end_cid > numchunks):
            end_cid = numchunks
        cidreqrange = str(st_cid) + ":" + str(end_cid)
        print "Requesting for ",cidreqrange
        try:
            sock = Xsocket(XSOCK_STREAM)
            
            status = Xconnect(sock, dag)
            if (status != 0):
            	print "Unexpected error:", sys.exc_info()[0]
        	Xclose(sock)
    		print "sendSIDRequestXSP() Closing browser socket "
    		browser_socket.close()
		return
            
            Xsend(sock, cidreqrange, len(cidreqrange), 0)
        except:
            print 'ERROR: xiaproxy.py: sendVideoSIDRequest: error requesting cidreqrange %s' % cidreqrange
	
	try:
	    reply= recv_with_timeout(sock) # = Xrecv(sock, 1024, 0)
    	except:
     	    print "closing browser socket8"
	    browser_socket.close()
	    break;

	Xclose(sock)
	#print reply
	if(i == 0):
    		send_to_browser(http_header, browser_socket)
	ret = process_videoCIDlist(reply, browser_socket, socks)
	if (ret==False):
	    break;
	## process CIDs 
    for i in range(threshold):
        Xclose(socks[i])
    return

def requestVideoCID(CID, fallback):
    sock = Xsocket(XSOCK_CHUNK)
    if (sock<0):
        print "error opening socket"
        return
    # Request content
    content_dag = 'CID:%s' % CID
    if fallback:
        content_dag = 'RE %s %s %s' % (AD1, HID1, content_dag)
    #print 'Retrieving content with ID: \n%s' % content_dag
    try:
        XrequestChunk(sock, content_dag, len(content_dag))
    except:
        print 'ERROR: xiaproxy.py: requestVideoCID: error requesting CID \n%s' % content_dag
    # Get content
    try:
        data = readcid_with_timeout(sock, content_dag)
        Xclose(sock)
    except:
        return  None
    return data

def getrandSID():
    sid = "SID:"+ ("%040d"% int(random.random()*1e40))
    assert len(sid)==44
    return  sid

def sendSIDRequestXSP(ddag, payload, browser_socket):
    print 'Sending SID Request to %s' % ddag
    # Create socket
    sock = Xsocket(XSOCK_STREAM)
    if (sock<0):
        print "ERROR: xiaproxy.py: sendSIDRequestXSP: could not open socket"
        return

    sid = getrandSID()
    sdag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP0, HID0, sid)    

    try:
        Xbind(sock, sdag)

        rtt = time.time() 
        # Connect to service
        status = Xconnect(sock, ddag)
        if (status != 0):
        	print "Unexpected error:", sys.exc_info()[0]
        	Xclose(sock)
    		print "sendSIDRequestXSP() Closing browser socket "
    		browser_socket.close()
		return
		       
        # Send request
        Xsend(sock, payload, len(payload), 0)
        
    except:
        print 'ERROR: xiaproxy.py: sendSIDRequestXSP: error binding to sdag, connecting to ddag, or sending SID request:\n%s' % payload

    # Receive reply and close socket
    try:
        print 'Trying to receiv CIDs from webserver'
        reply= recv_with_timeout(sock) # Use default timeout
        print 'Reply: \n %s' % reply
    except IOError:
        print "Unexpected error:", sys.exc_info()[0]
        Xclose(sock)
        print "sendSIDRequestXSP() Closing browser socket "
        browser_socket.close()
        return 
    Xclose(sock)

    contains_CID = check_for_and_process_CIDs(reply, browser_socket)
    if not contains_CID:
        # Pass reply up to browswer 
        rtt = int((time.time()-rtt) *1000)
        # Use last modified field to embedd RTT info
        reply = reply.replace("Last-Modified: 100", ("Last-Modified:%d" % rtt)) # TODO: a bit of a hack
        send_to_browser(reply, browser_socket)
    return





# This fuction is just for testing purpose
def sendSIDRequestXDP(ddag, payload, browser_socket):
    # Create socket
    sock = Xsocket(XSOCK_DGRAM)
    if (sock<0):
        print "ERROR: xiaproxy.py: sendSIDRequestXDP: could not open socket"
        return

    sid = getrandSID()
    sdag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP0, HID0, sid)    
    replyto =  ''
    reply_dag = ''

    try:

        rtt = time.time() 
        
        # Send request
        Xsendto(sock, payload, len(payload), 0, ddag, len(ddag)+1)
        
    except:
        print 'ERROR: xiaproxy.py: sendSIDRequestXDP: error binding to sdag, or sending SID request:\n%s' % payload

    # Receive reply and close socket
    try:
        
        (reply, reply_dag) = recvfrom_with_timeout(sock) # Use default timeout
        #print "xiaproxy.py: reponse: %s" % reply
    except IOError:
        print "Unexpected error:", sys.exc_info()[0]
        Xclose(sock)
        print "sendSIDRequestXDP() Closing browser socket "
        browser_socket.close()
        return 
    Xclose(sock)
    
    # Pass reply up to browswer 
    rtt = int((time.time()-rtt) *1000)
    # Use last modified field to embedd RTT info
    reply = reply.replace("Last-Modified: 100", ("Last-Modified:%d" % rtt)) # TODO: a bit of a hack
    send_to_browser(reply, browser_socket)
    return    
       

# As the name suggests, this function is only temporary; ultimately we want to use XrequestChunkList
# instead of repeated calls to XrequestChunk
def get_content_from_cid_list_temp(cid_list):
    num_cids = len(cid_list) / 40
    

    # make a socket
    sock = Xsocket(XSOCK_CHUNK)
    if (sock<0):
        print "ERROR: xiaproxy.py: get_content_from_cid_list: error opening socket"
        return

    # create and bind to ephemeral SID
    sid = getrandSID()
    sdag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP0, HID0, sid)       
    try:
        Xbind(sock, sdag);
    except:
        print 'ERROR: xiaproxy.py: get_content_from_cid_list: Error binding to sdag'

    # request each chunk of content individually
    content = ""
    for i in range(0, num_cids):
        content_dag = 'CID:%s' % cid_list[i*40:40+i*40]
        content_dag = "DAG 3 0 1 - \n %s 3 2 - \n %s 3 2 - \n %s 3 - \n %s" % (AD1, IP1, HID1, content_dag)
        try:
            XrequestChunk(sock, content_dag, len(content_dag))
        except:
            print 'ERROR: xiaproxy.py: get_content_from_cid_list_temp: Problem requesting chunk\n%s' % content_dag
        content += readcid_with_timeout(sock, content_dag)

    Xclose(sock)
    return content


# Due to API changes, this function isn't working yet; use get_content_from_cid_list_temp for now.
# There is a problem with ChunkStatuasArray's (the python wrapper for ChunkStatus*)
def get_content_from_cid_list(cid_list):
    num_cids = len(cid_list) / 40
    
    # make a list of ChunkStatuss
    cids = ChunkStatusArray(num_cids) # list()
    cids_temp = [] # quick workaround since th ChunkStatusArray seems to have stopped working
    statuses_temp = []
    for i in range(0, num_cids):
        content_dag = 'CID:%s' % cid_list[i*40:40+i*40]
        content_dag = "DAG 3 0 1 - \n %s 3 2 - \n %s 3 2 - \n %s 3 - \n %s" % (AD1, IP1, HID1, content_dag)
        cids_temp.append(content_dag)
        chunk_info = ChunkStatus()
        chunk_info.cDAG = content_dag
        chunk_info.dlen = len(content_dag)
        chunk_info.status = 0
        cids[i] = chunk_info
        statuses_temp.append(chunk_info)

    # make a socket
    sock = Xsocket(XSOCK_CHUNK)
    if (sock<0):
        print "ERROR: xiaproxy.py: get_content_from_cid_list: error opening socket"
        return

    # create and bind to ephemeral SID
    sid = getrandSID()
    sdag = "DAG 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (AD0, IP0, HID0, sid)       
    try:
        Xbind(sock, sdag);
    except:
        print 'ERROR: xiaproxy.py: get_content_from_cid_list: Error binding to sdag'

    # request the list of CIDs
    try:
        XrequestChunks(sock, cids, num_cids)
    except:
        print 'ERROR: xiaproxy.py: get_content_from_cid_list: Error requesting CID list'

    # read CIDs as they become available
    content = ""
    for i in range(0, num_cids):
        data = readcid_with_timeout(sock, cids[i].cDAG)
        content += data

    Xclose(sock)
    return content

def xiaHandler(host, path, http_header, browser_socket):
    # Configure XSocket so we can talk to click
    set_conf("xsockconf_python.ini", "xiaproxy.py")
    

    if http_header.find('GET /favicon.ico') != -1:
        return
    if host.find('dag') == 0:
        # Get the DAG from the URL
        ddag = dag_from_url('http://' + host + path)
        # Remove the DAG from the request so only the requested page remains
        sendSIDRequestXSP(ddag, http_header, browser_socket)
    elif host.find('sid') == 4:
        host=host[4:]  # remove the 'xia.' prefix
        # TODO: is it necessary to handle video service requests separately?
        found_video = host.find('video')
        if(found_video != -1):
            sendVideoSIDRequest(host[4:], http_header, browser_socket)
        elif(host.find('sid_stock') != -1):
            # For stock_service, use XDP (for testing purpose)
            ddag = dag_from_url_old(host + path)
            # If there's a fallback in the filename, remove it now (TODO: change this when we switch to new URL format)
            http_header = re.sub(r"/fallback\(\S*\)", "", http_header)
            sendSIDRequestXDP(ddag, http_header, browser_socket)               
        else:
            # Do some URL processing 
            ddag = dag_from_url_old(host + path)
            # If there's a fallback in the filename, remove it now (TODO: change this when we switch to new URL format)
            http_header = re.sub(r"/fallback\(\S*\)", "", http_header)
            sendSIDRequestXSP(ddag, http_header, browser_socket)
    elif host.find('cid') == 4:
        host=host[4:]  # remove the 'xia.' prefix
        host_array = host.split('.')
        num_chunks = int(host_array[1])
        recombined_content = get_content_from_cid_list_temp(host_array[2])
        length = len(recombined_content)
        send_to_browser(recombined_content, browser_socket)
    return

