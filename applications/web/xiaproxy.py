import socket, select, random
import struct, time, signal, os, sys, re
import threading
import Tkinter
from tkMessageBox import showwarning
from c_xsocket import *
from ctypes import *
from xia_address import *

XSP=1
XDP=2
XCHUNKP=3

def send_to_browser(data, browser_socket):
    try:
        browser_socket.send(data)
        return True
    except:
        print 'ERROR: xiaproxy.py: send_to_browser: error sending data to browser'
        browser_socket.close()
        return False

def warn_bad_content():
    #os.system("python bad_content_warning.py")
    thread = threading.Thread(target=os.system, args=("python bad_content_warning.py",))
    thread.start()
    #thread = threading.Thread(target=showwarning, args=("Invalid Content Hash", "Firefox received bad content"))
    #thread.start()
    #result = showwarning("Invalid Content Hash", "Firefox received content that does not match the reqeusted CID.")

def recv_with_timeout(sock, timeout=5, transport_proto=XSP):
    # Receive data
    start_time = time.time()   # current time in seconds since the epoch
    received_data = False
    reply = '<html><head><title>XIA Error</title></head><body><p>&nbsp;</p><p>&nbsp;</p><p style="text-align: center; font-family: Tahoma, Geneva, sans-serif; font-size: xx-large; color: #666;">Sorry, something went wrong.</p><p>&nbsp;</p><p style="text-align: center; color: #999; font-family: Tahoma, Geneva, sans-serif;"><a href="mailto:xia-dev@cs.cmu.edu">Report a bug</a></p></body></html>'
    try:
        while (time.time() - start_time < timeout and not received_data):
            try:
                select.select([sock], [], [], 0.02)
                if transport_proto == XSP:
                    reply = Xrecv(sock, XIA_MAXBUF, 0)
                elif transport_proto == XDP:
                    (reply, reply_dag) = Xrecvfrom(sock, XIA_MAXBUF, 0)
                received_data = True
            except IOError:
                received_data = False
            except Exception, msg:
                print 'ERROR: xiaproxy.py: recv_with_timeout: %s' % msg
    except (KeyboardInterrupt, SystemExit), e:
        Xclose(sock)
        sys.exit()

    if (not received_data):
        print "Recieved nothing"
        raise IOError
        
    if transport_proto == XSP:
        return reply
    elif transport_proto == XDP:
    	return reply, reply_dag
    
    
def readcid_with_timeout(sock, cid, timeout=0.1):
    # Receive data
    start_time = time.time()   # current time in seconds since the epoch
    received_data = False
    reply = '<html><head><title>XIA Error</title></head><body><p>&nbsp;</p><p>&nbsp;</p><p style="text-align: center; font-family: Tahoma, Geneva, sans-serif; font-size: xx-large; color: #666;">Sorry, something went wrong.</p><p>&nbsp;</p><p style="text-align: center; color: #999; font-family: Tahoma, Geneva, sans-serif;"><a href="mailto:xia-dev@cs.cmu.edu">Report a bug</a></p></body></html>'
    try:
        while (time.time() - start_time < timeout and not received_data):
            try:
                status = XgetChunkStatus(sock, cid)
                if status & READY_TO_READ == READY_TO_READ or status & INVALID_HASH == INVALID_HASH:
                    reply = XreadChunk(sock, 65521, 0, cid)
                    received_data = True
                    if status & INVALID_HASH == INVALID_HASH:
                        warn_bad_content()
                        return False
            except IOError:
                received_data = False
            except Exception, msg:
                print 'ERROR: xiaproxy.py: readcid_with_timeout: %s' % msg
    except (KeyboardInterrupt, SystemExit), e:
        Xclose(sock)
        sys.exit()

    if (not received_data):  # TIMEOUT
        print "%s: Recieved nothing; requesting retransmit" % time.time()
        XrequestChunk(sock, cid)
        return readcid_with_timeout(sock, cid)
        raise IOError

    return reply
    
    

def check_for_and_process_CIDs(dstAD, dst4ID, dstHID, message, browser_socket):
    rt = message.find('cid')
    if (rt!= -1):
        http_header = message[0:rt]
        try:
            content = get_content_from_cid_list(dstAD, dst4ID, dstHID, message[rt:].split('.')[2])
        except:
            print "ERROR: xiaproxy.py: check_for_and_process_CIDs: Couldn't retrieve content. Closing browser_socket"
            browser_socket.close()
            return True
        send_to_browser(http_header, browser_socket)
        send_to_browser(content, browser_socket)
        return True
    else:
        return False

def process_videoCIDlist(dstAD, dstHID, message, browser_socket, socks):
    rt = message.find('CID') 
    cidlist = list()
    while(rt != -1):
	CID = message[rt+4:rt+44]
	content_dag = 'CID:%s' % CID
        #content_dag = 'RE %s %s %s' % (AD1, HID1, content_dag)
        content_dag = 'DAG 2 0 - \n %s 2 1 - \n %s 2 - \n %s' % (dstAD, dstHID, content_dag)
	cidlist.append(content_dag)
        #XrequestChunk(moresock, content_dag)
        #content = Xrecv(moresock, 65521, 0)
	#browser_socket.send(content)
	rt = message.find('CID', rt+44)
    ## issue multiple request
    ## and receive multiple content
    ## first issue all the requests
    for i in range(len(cidlist)):
        try:
            XrequestChunk(socks[i], cidlist[i])
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


 
def sendVideoSIDRequest(ddag, payload, browser_socket):

    sock = Xsocket(XSOCK_STREAM)
    if (sock<0):
        print "error opening socket"
        return
  
    status = Xconnect(sock, ddag)
    if (status != 0):
       	print "Unexpected error:", sys.exc_info()[0]
       	Xclose(sock)
    	print "send_sid_request() Closing browser socket "
    	browser_socket.close()
	return
    
    print "Connected. OK\n"
    # Send request for number of chunks
    asknumchunks = "numchunks";
    Xsend(sock, asknumchunks, 0)
    #Xsend(sock, payload, 0)
    # Receive reply
    print 'send_sid_request: about to receive reply'
    try:
        reply = recv_with_timeout(sock) # = Xrecv(sock, 65521, 0)
    except:
        Xclose(sock)
    	print "closing browser socket7"
	browser_socket.close()
	return

    Xclose(sock)
    numchunks = int(reply)
    print "send_sid_request: received reply for number of chunks ",numchunks

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
            
            status = Xconnect(sock, ddag)
            if (status != 0):
            	print "Unexpected error:", sys.exc_info()[0]
        	Xclose(sock)
    		print "send_sid_request() Closing browser socket "
    		browser_socket.close()
		return
            
            Xsend(sock, cidreqrange, 0)
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
    	# Extract dst AD and HID from ddag	
   	start_index = ddag.find('AD:')
    	dstAD = ddag[start_index:start_index+3+40] 
    	start_index = ddag.find('HID:')
    	dstHID = ddag[start_index:start_index+4+40]  	
	
	if(i == 0):
    		send_to_browser(http_header, browser_socket)
	ret = process_videoCIDlist(dstAD, dstHID, reply, browser_socket, socks)
	if (ret==False):
	    break;
	## process CIDs 
    for i in range(threshold):
        Xclose(socks[i])
    return

def requestVideoCID(dstAD, dstHID, CID, fallback):
    sock = Xsocket(XSOCK_CHUNK)
    if (sock<0):
        print "error opening socket"
        return
    # Request content
    content_dag = 'CID:%s' % CID
    if fallback:
        content_dag = 'RE %s %s %s' % (dstAD, dstHID, content_dag)
    #print 'Retrieving content with ID: \n%s' % content_dag
    try:
        XrequestChunk(sock, content_dag)
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

def send_sid_request(ddag, payload, browser_socket, transport_proto=XSP):
    # Create socket
    if transport_proto == XSP:
        sock = Xsocket(XSOCK_STREAM)
    elif transport_proto == XDP:
        sock = Xsocket(XSOCK_DGRAM)
    else:
        print "ERROR: xiaproxy.py: send_sid_request: Bad transport protocol specified"
        return

    if (sock<0):
        print "ERROR: xiaproxy.py: send_sid_request: could not open socket"
        return
    
    try:
        if transport_proto == XSP:
            # Connect to service
            status = Xconnect(sock, ddag)
            if (status != 0):
                print "send_sid_request() Closing browser socket "
                print "Unexpected error:", sys.exc_info()[0]
                Xclose(sock)
                browser_socket.close()
                return

        rtt = time.time() 

        # Send request
        if transport_proto == XSP:
            Xsend(sock, payload, 0)
        elif transport_proto == XDP:
            Xsendto(sock, payload, 0, ddag)
    except IOError:
        print 'ERROR: xiaproxy.py: send_sid_request: error binding to sdag, connecting to ddag, or sending SID request:\n%s' % payload
        

    # Receive reply and close socket
    try:
        if transport_proto == XSP:
            reply = ''
            while reply.find('DONEDONEDONE') < 0:
                reply += recv_with_timeout(sock) # Use default timeout
            reply = reply.split('DONEDONEDONE')[0]
        elif transport_proto == XDP:
            (reply, reply_dag) = recv_with_timeout(sock, 5, XDP)
    except IOError:
        print "ERROR: xiaproxy.py: send_sid_request(): Closing browser socket "
        print "Unexpected error:", sys.exc_info()[0]
        Xclose(sock)
        browser_socket.close()
        return 

    if transport_proto == XSP:
        Xclose(sock)

    # Extract dst AD and HID from ddag	
    start_index = ddag.find('AD:')
    dstAD = ddag[start_index:start_index+3+40] 
    start_index = ddag.find('IP:')
    dst4ID = ddag[start_index:start_index+3+40] 
    start_index = ddag.find('HID:')
    dstHID = ddag[start_index:start_index+4+40]  

    contains_CID = check_for_and_process_CIDs(dstAD, dst4ID, dstHID, reply, browser_socket)
    if not contains_CID:
        # Pass reply up to browswer 
        rtt = int((time.time()-rtt) *1000)
        # Use last modified field to embedd RTT info
        reply = reply.replace("Last-Modified: 100", ("Last-Modified:%d" % rtt)) # TODO: a bit of a hack
        send_to_browser(reply, browser_socket)
    return


def get_content_from_cid_list(dstAD, dst4ID, dstHID, cid_list):
    num_cids = len(cid_list) / 40
    
    # make a list of ChunkStatuss
    cids = ChunkStatusArray(num_cids) # list()
    for i in range(0, num_cids):
        cid = 'CID:%s' % cid_list[i*40:40+i*40]
        content_dag = "DAG 3 0 1 - \n %s 3 2 - \n %s 3 0 - \n %s 3 - \n %s" % (dstAD, dst4ID, dstHID, cid)
        
        chunk_info = ChunkStatus()
        chunk_info.cid = content_dag
        chunk_info.cidLen = len(content_dag)
        chunk_info.status = 0

        cids[i] = chunk_info

    # make a socket
    sock = Xsocket(XSOCK_CHUNK)
    if (sock<0):
        print "ERROR: xiaproxy.py: get_content_from_cid_list: error opening socket"
        return

    # request the list of CIDs
    try:
        XrequestChunks(sock, cids, num_cids)
    except:
        print 'ERROR: xiaproxy.py: get_content_from_cid_list: Error requesting CID list'

    # read CIDs as they become available
    content = ""
    for i in range(0, num_cids):
        data = readcid_with_timeout(sock, cids[i].cid)
        if data == False:  ## HACK FOR GEC!! REMOVE ME!!
            with open('anon.jpg', 'r') as f:
                anon_data = f.read()
            f.closed
            Xclose(sock)
            return anon_data
        content += data

    Xclose(sock)
    return content

def xia_handler(host, path, http_header, browser_socket):
    # Configure XSocket so we can talk to click
    set_conf("xsockconf_python.ini", "xiaproxy.py")

    if http_header.find('GET /favicon.ico') != -1:
        return
    if host.find('dag') == 0:
        # Get the DAG from the URL
        ddag = dag_from_url('http://' + host + path)
        # Remove the DAG from the request so only the requested page remains
        send_sid_request(ddag, http_header, browser_socket)
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
            send_sid_request(ddag, http_header, browser_socket, XDP)          
        else:
            # Do some URL processing 
            ddag = dag_from_url_old(host + path)
            # If there's a fallback in the filename, remove it now (TODO: change this when we switch to new URL format)
            http_header = re.sub(r"/fallback\(\S*\)", "", http_header)
            send_sid_request(ddag, http_header, browser_socket)
    elif host.find('cid') == 4:
    	# format: "xia.cid.#cids.AD.xxx.HID.xxx.CIDLISTxxx"
        host=host[8:]  # remove the 'xia.cid.' prefix
        host_array = host.split('.')
        num_chunks = int(host_array[0])
        dstAD = "AD:%s" % host_array[2]
        dst4ID = "IP:%s" % host_array[4]
        dstHID = "HID:%s" % host_array[6]
        recombined_content = get_content_from_cid_list(dstAD, dst4ID, dstHID, host_array[7])
        length = len(recombined_content)
        send_to_browser(recombined_content, browser_socket)
    else:
        ddag = XgetDAGbyName(host)
        if ddag == None:
            print 'xiaproxy.py: xia_handler: Could not resolve name %s' % host
            return
        if host.find('www_s.stock.com.xia') != -1:  
            send_sid_request(ddag, http_header, browser_socket, XDP)
        elif host.find('www_s.video.com.xia') != -1:   
            sendVideoSIDRequest(ddag, http_header, browser_socket)          
        elif host.find('www_s.') != -1:     	
            send_sid_request(ddag, http_header, browser_socket)
        elif host.find('www_c.') != -1:     	
    	    # Extract dst AD, 4ID, HID, and CID from ddag	
    	    ad_index = ddag.find('AD:')
            dstAD = ddag[ad_index:ad_index+3+40] 
    	    ip_index = ddag.find('IP:')
            dst4ID = ddag[ip_index:ip_index+3+40] 
            hid_index = ddag.find('HID:')
            dstHID = ddag[hid_index:hid_index+4+40]  
            cid_index = ddag.find('CID:')
            dstCID = ddag[cid_index+4:cid_index+4+40] 
            recombined_content = get_content_from_cid_list(dstAD, dst4ID, dstHID, dstCID)
            length = len(recombined_content)
            send_to_browser(recombined_content, browser_socket)
    return

