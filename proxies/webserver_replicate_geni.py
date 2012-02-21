#!/usr/bin/python
    
import socket
import sys
import struct
import time
import datetime
import os
from xsocket import *
from ctypes import *
import hashlib

chunksize = 1200

# Pretend a magic naming service gives us XIDs...
from xia_address import *

# TODO: This should eventually be replaced by the putCID API
def putCID(chunk):
    # Hash the content to get CID
    m = hashlib.sha1()
    m.update(chunk)
    cid = m.hexdigest()

    sock = Xsocket(XSOCK_STREAM)
    if (sock<0):
        print "webserver.py: putCID: error opening socket"
        exit(-1)
    
    # Put the content chunk
    content_dag = 'RE %s %s CID:%s' % (AD1, HID1, cid)
    XputCID(sock, chunk, len(chunk), 0, content_dag, len(content_dag))

    print 'put content %s (length %s)' % (content_dag, len(chunk))
    Xclose(sock)
    return cid

def serveHTTPRequest(request, sock):
    # Make sure this is an HTTP GET request
    if request.find('GET') != 0:
        print 'WARNING: webserver.py: serveHTTPRequest: Received an HTTP request other than GET:\n%s' % request
        return
    
    # Make HTTP header
    date = datetime.datetime.now().strftime("%a, %d %b %Y %H:%M:%S %Z")  #TODO: fix time zone
    file_data = ''
    http_msg_type = ''
    http_header = 'Date: %s\nServer: XIA Baby Webserver\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type: text/html\n\n' % date

    # If file exists, read it into memory; otherwise return 404 Not Found
    requested_file = request.split(' ')[1][1:]
    requested_file = requested_file.split('/')[0]
    f = None    
    print 'request: %s' %(request)
    print 'requested file: %s' %(requested_file)
    try:
        f = open(requested_file, 'r')
        file_data = f.read()
        http_msg_type = 'HTTP/1.1 200 OK\n'
    except IOError:
        print 'ERROR: webserver.py: serverHTTPRequest: Could not read requested file: %s' % requested_file
        file_data = '<html><body><h1>Sorry, we can\'t find that page.</h1></body></html>'
        http_msg_type = "HTTP/1.1 404 Not Found\n"
    finally:
        if f:
            f.close()

    # Send response
    response = http_msg_type + http_header + file_data
    Xsend(sock, response, len(response), 0)
    

def put_content():
    # Put content 'image.jpg' and make a corresponding list of CIDs
    # (if image is chunked it might have multiple CIDs)
    image_cid_list = []
    f = open("image.jpg", 'r')
    chunk = f.read(chunksize)
    while chunk != '':
        image_cid_list.append(putCID(chunk))
        chunk = f.read(chunksize)
    f.close()

    # Build 'simple.html' file
    num_image_chunks = len(image_cid_list)
    image_cid_list_string = ''

    for cid in image_cid_list:
        image_cid_list_string += cid
    f = open("simple.html", 'w')
    f.write('<html><body><h1>It works!</h1>\n<h2><img src="http://xia.cid.%s.%s" /></h2><ul class="left-nav">\n\n</body></html>' % (num_image_chunks, image_cid_list_string))
    f.close()

    # Put content 'plane.jpg'
    f = open("plane.jpg", 'r')
    chunk = f.read(chunksize)

    while chunk != '':
        cid = putCID(chunk)
        chunk = f.read(chunksize)
    f.close()


def main():
    # Set up connection with click via Xsocket API
    set_conf("xsockconf_python.ini", "webserver_replicate_geni.py")

    # TODO: When new putCID API is ready and we have persistent caching, we can eliminate
    # this and make a separate 'content publishing' app.
    try:
        sys.argv.index('-r') # don't republish content if we're restarting but didn't restart click
        print 'Restarting webserver. Don\'t republish content'
    except:
        put_content() # '-r' not found, so do publish content

    try:   
        # Listen for connections from clients
        listen_sock = Xsocket(XSOCK_STREAM)
        if (listen_sock<0):
            print 'webserver.py: main: error opening socket'
            return
        dag = "RE %s %s %s" % (AD1, HID1, SID_STOCK_INFO) # dag to listen on
        Xbind(listen_sock, dag)
        print 'Listening on %s' % dag
        
        # TODO: use threads instead of processes?
        while(True):
            accept_sock = Xaccept(listen_sock);
            child_pid = os.fork()
  
            if child_pid == 0:  
                incoming_data = Xrecv(accept_sock, 65521, 0)
                serveHTTPRequest(incoming_data, accept_sock)
                Xclose(accept_sock)
                os._exit(0)
    except (KeyboardInterrupt, SystemExit), e:
       print 'Closing webserver'
       Xclose(listen_sock)
       sys.exit()
    


if __name__ ==  '__main__':
    main()    

