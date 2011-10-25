#!/usr/bin/python

import socket
import sys
import struct
import time
import os
import xsocket
from ctypes import *

chunksize = 65536
CID = ['0000000000000000000000000000000000000000', '0000000000000000000000000000000000000001', '0000000000000000000000000000000000000010','0000000000000000000000000000000000000011','0000000000000000000000000000000000000100', '0000000000000000000000000000000000000101', '0000000000000000000000000000000000000110','0000000000000000000000000000000000000111','0000000000000000000000000000000000001000', '0000000000000000000000000000000000001001', '0000000000000000000000000000000000001010', '0000000000000000000000000000000000001011']
cid_i = 0
length = 0

# Pretend a magic naming service gives us XIDs...
HID0= "HID:0000000000000000000000000000000000000000"
HID1= "HID:0000000000000000000000000000000000000001"
AD0=  "AD:1000000000000000000000000000000000000000"
AD1=  "AD:1000000000000000000000000000000000000001"
RHID0="HID:0000000000000000000000000000000000000002"
RHID1="HID:0000000000000000000000000000000000000003"
SID1= "SID:0f00000000000000000000000000000000000056"
CID0= "CID:2000000000000000000000000000000000000001"
CID_TEST_HTML = "CID:0000000000000000000000000000000000000000"

def putCID(chunk):
    global cid_i
    #TODO: Actually compute hashes, don't use global array
    sock = xsocket.Xsocket()
    if (sock<0):
        print "error opening socket"
        return
    
    # Put the content chunk
    content_dag = 'RE %s %s CID:%s' % (AD1, HID1, CID[cid_i])
    xsocket.XputCID(sock, chunk, len(chunk), 0, content_dag, len(content_dag))

    print 'put content %s (length %s)' % (content_dag, len(chunk))
    xsocket.Xclose(sock)

    cid_i += 1
    return

def serveSIDRequest(request, sock):
    # For now, regardless of what was requested, we respond
    # with an HTTP message directing the browser to request
    # the html file at CID_TEST_HTML
    response = 'HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nLast-Modified: Sat, 08 Jan 2011 21:08:31 GMT\nCache-Control: no-cache\nAccept-Ranges: bytes\nContent-Length: ' + str(length) + '\nConnection: close\nContent-Type: text/html\n\n'+ CID_TEST_HTML
    print 'Response:\n%s' % response
    xsocket.Xsend(sock, response, len(response), 0)
    return

def main():
    global AD1, HID1, SID1

    # Set up connection with click via Xsocket API
    xsocket.set_conf("xsockconf_python.ini", "webserver.py")
    xsocket.print_conf()  #for debugging

    # Put content 'test.html'
    f = open("test.html", 'r')
    chunk = f.read(chunksize)
    global length
    length = len(chunk)
    putCID(chunk)
    f.close()

    # Put content 'image.jpg'
    f = open("image.jpg", 'r')
    chunk = f.read(chunksize)
    while chunk != '':
        putCID(chunk)
        chunk = f.read(chunksize)
    f.close()
    
    time.sleep(1) #necessary?

    # Now listen for connections from clients
    listen_sock = xsocket.Xsocket()
    if (listen_sock<0):
        print 'error opening socket'
        return
    dag = "RE %s %s %s" % (AD1, HID1, SID1) # dag to listen on
    xsocket.Xbind(listen_sock, dag)
    print 'Listening on %s' % dag

    while True:
        xsocket.Xaccept(listen_sock)
        incoming_data = xsocket.Xrecv(listen_sock, 1024, 0)
        print incoming_data
        serveSIDRequest(incoming_data, listen_sock)
    


if __name__ ==  '__main__':
    main()

