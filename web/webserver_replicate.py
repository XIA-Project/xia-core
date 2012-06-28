#!/usr/bin/python

import socket
import sys
import struct
import time
import os
import c_xsocket
from ctypes import *
import hashlib

chunksize = 1200
#CID = ['0000000000000000000000000000000000000000', '0000000000000000000000000000000000000001', '0000000000000000000000000000000000000010','0000000000000000000000000000000000000011','0000000000000000000000000000000000000100', '0000000000000000000000000000000000000101', '0000000000000000000000000000000000000110','0000000000000000000000000000000000000111','0000000000000000000000000000000000001000', '0000000000000000000000000000000000001001', '0000000000000000000000000000000000001010', '0000000000000000000000000000000000001011']
#cid_i = -1
length = 0

# Pretend a magic naming service gives us XIDs...
from xia_address import *

CID_SIMPLE_HTML = ""  # we set this when we 'put' the hmtl page
CID_DEMO_HTML = ""  # we set this when we 'put' the hmtl page
CID_ONLY_STOCK_SEVICE_HTML = ""  # we set this when we 'put' the hmtl page

def putCID(chunk):
    #global cid_i
    #cid_i += 1

    # Hash the content to get CID
    m = hashlib.sha1()
    m.update(chunk)
    cid = m.hexdigest()

    print 'waiting to get socket'
    sock = xsocket.Xsocket(0)
    if (sock<0):
        print "error opening socket"
        exit(-1)
    print 'got socket'
    
    print 'waiting to put content'
    # Put the content chunk
    content_dag = 'RE %s %s CID:%s' % (AD_CMU, HID2, cid)
    xsocket.XputCID(sock, chunk, len(chunk), 0, content_dag, len(content_dag))

    print 'put content %s (length %s)' % (content_dag, len(chunk))
    print cid
    xsocket.Xclose(sock)

    return cid

def serveSIDRequest(request, sock):
    # Respond with either CID_DEMO_HTML or CID_DEMO_HTML or CID_ONLY_STOCK_SEVICE_HTML
    # TODO: This code should be better
    
    # To prevent cid referenced before assignment
    cid = CID_SIMPLE_HTML
    if request.find('simple.html') >= 0:
        cid = CID_SIMPLE_HTML
    elif request.find('demo.html') >= 0:  
        cid = CID_DEMO_HTML
    elif request.find('xia_service.html') >= 0:  
        cid = CID_ONLY_STOCK_SEVICE_HTML
    
    response = 'HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type: text/html\n\n'+ cid
    print 'Webserver Response:\n%s' % response
    xsocket.Xsend(sock, response, len(response), 0)
    return


def put_content():
    global length
    global CID_SIMPLE_HTML, CID_DEMO_HTML, CID_ONLY_STOCK_SEVICE_HTML
    
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

    # Put content 'simple.html'
    # TODO: Silly to write file then read it again; we do it
    # for now so we can see the actual file for debugging
    print "simple.html"
    f = open("simple.html", 'r')
    chunk = f.read(chunksize)

    while chunk != '':
        cid = putCID(chunk)
        CID_SIMPLE_HTML += 'CID:' 
	CID_SIMPLE_HTML  += cid
        chunk = f.read(chunksize)
    f.close()
    print "end simple.html"

    # Put content 'demo.html'
    print "demo.html"
    f = open("demo.html", 'r')
    chunk = f.read(chunksize)

    while chunk != '':
        cid = putCID(chunk)
        CID_DEMO_HTML += 'CID:' 
	CID_DEMO_HTML  += cid
	CID_DEMO_HTML  += '\t'
        chunk = f.read(chunksize)
    f.close()
    
    print "end demo.html"
    
    # Put content 'plane.jpg'
    print "plane.jpg"
    f = open("plane.jpg", 'r')
    chunk = f.read(chunksize)

    while chunk != '':
        cid = putCID(chunk)
        chunk = f.read(chunksize)
    f.close()
    print "end plane.jpg"


    # Put content 'xia_service.html'
    print "xia_service.html"
    f = open("xia_service.html", 'r')
    chunk = f.read(chunksize)

    while chunk != '':
        cid = putCID(chunk)
        CID_ONLY_STOCK_SEVICE_HTML += 'CID:' 
	CID_ONLY_STOCK_SEVICE_HTML  += cid
	CID_ONLY_STOCK_SEVICE_HTML  += '\t'
        chunk = f.read(chunksize)
    f.close()
    
    print "end xia_service.html"


def main():
    global AD_CMU, HID2, SID1

    print 'starting webserver'
    # Set up connection with click via Xsocket API
    xsocket.set_conf("xsockconf_python.ini", "webserver_replicate.py")
    xsocket.print_conf()  #for debugging

    try:
        sys.argv.index('-r') # don't republish content if we're restarting but didn't restart click
        print 'Restarting webserver. Don\'t republish content'
    except:
        put_content() # '-r' not found, so do publish content
    time.sleep(1) #necessary?

    while True:
        try:   
            # Now listen for connections from clients
            print 'webserver_replicate.py: Waiting to get socket to listen on'
            listen_sock = xsocket.Xsocket(0)
            if (listen_sock<0):
                print 'error opening socket'
                return
            dag = "RE %s %s %s" % (AD_CMU, HID2, SID1) # dag to listen on
            xsocket.Xbind(listen_sock, dag)
            print 'Listening on %s' % dag

            xsocket.Xaccept(listen_sock)
            incoming_data = xsocket.Xrecv(listen_sock, 2000, 0)
            print "webserver got %s" % incoming_data
            serveSIDRequest(incoming_data, listen_sock)
        except (KeyboardInterrupt, SystemExit), e:
            print 'Closing webserver'
            xsocket.Xclose(sock)
            sys.exit()
    


if __name__ ==  '__main__':
    main()

