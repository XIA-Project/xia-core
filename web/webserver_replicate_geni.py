#!/usr/bin/python

import socket
import sys
import struct
import time
import datetime
import os
import string
from xsocket import *
from ctypes import *
import hashlib

chunksize = 1200
cids_by_filename = {}

# Pretend a magic naming service gives us XIDs...
from xia_address import *

# TODO: This should eventually be replaced by the put_chunk API
def put_chunk(chunk):
    # Hash the content to get CID
    m = hashlib.sha1()
    m.update(chunk)
    cid = m.hexdigest()

    sock = Xsocket(XSOCK_CHUNK)
    if (sock<0):
        print "webserver.py: put_chunk: error opening socket"
        exit(-1)
    
    # Put the content chunk
    content_dag = 'RE %s %s CID:%s' % (AD1, HID1, cid)  # TODO: test DAG format instead of RE
    XputCID(sock, chunk, len(chunk), 0, content_dag, len(content_dag))

    print 'put content %s (length %s)' % (content_dag, len(chunk))
    Xclose(sock)
    return cid

def put_file(filepath, chunksize):
    print "putting file %s" % filepath
    cid_list = []
    try:
        f = open(filepath, 'r')
        chunk = f.read(chunksize)
        while chunk != '':
            cid_list.append(put_chunk(chunk))
            chunk = f.read(chunksize)
    except IOError:
        print "IOERROR: webserver.py: put_file: error reading file %s" % filepath
    finally:
        if f:
            f.close()
    return cid_list
    

def serveHTTPRequest(request, sock):
    global cids_by_filename
    # Make sure this is an HTTP GET request
    if request.find('GET') != 0:
        print 'WARNING: webserver.py: serveHTTPRequest: Received an HTTP request other than GET:\n%s' % request
        return
    
    # Make HTTP header
    date = datetime.datetime.now().strftime("%a, %d %b %Y %H:%M:%S %Z")  #TODO: fix time zone
    response_data = ''
    http_msg_type = ''
    http_header = 'Date: %s\nServer: XIA Baby Webserver\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type: text/html\n\n' % date

    # If file exists in cids_by_filename, return its CID (list); otherwise return 404 Not Found
    requested_file = './www_replicate/' + request.split(' ')[1][1:]
    try:
        cid_list = cids_by_filename[requested_file]
        cid_string = 'cid.%i.' % len(cid_list)
        for cid in cid_list:
            cid_string += cid

        response_data = cid_string
        http_msg_type = 'HTTP/1.1 200 OK\n'
    except KeyError:
        print 'ERROR: webserver.py: serverHTTPRequest: Could not find requested file: %s' % requested_file
        response_data = '<html><body><h1>Sorry, we can\'t find that page.</h1></body></html>'
        http_msg_type = "HTTP/1.1 404 Not Found\n"

    # Send response
    response = http_msg_type + http_header + response_data
    Xsend(sock, response, len(response), 0)

# Chunk and publish all files in the local www_replicate directory.
# If the file is an html file and contains images also stored in
# the local www_replicate directory, publish those images first and replace
# the images' URLs in the web chunks we publish with the list of
# CIDs we just made
def put_content_in_dir(dir):
    global cids_by_filename
    if not os.path.exists(dir):
        print 'ERROR: webserver.py: put_content_in_dir: Directory "%s" does not exist.' % dir
        return

    # PASS 1:
    # publish each non-html file in the given directory; keep a dictionary 
    # mapping filepath to the corresponding list of CIDs
    cids_by_filename = {}
    html_files = []
    for root, dirs, files in os.walk(dir):
        for file in files:
            if file[-5:] != '.html' and file[-9:] != '.htmlTEMP':
                cids_by_filename[os.path.join(root,file)] = put_file(os.path.join(root, file), 1200)
            else:
                html_files.append(os.path.join(root, file))

    print html_files
    # PASS 2:
    # for each html file, find references to any of the files we just
    # published as content. Replace the reference with the corresponding
    # CID list. Replace references to other local html files with DAG
    # pointing to this webserver. Then publish the modified html file. 
    cids_by_filename_to_add = {} # need a temp dict here because we can't add to cids_by_filename while we're iterating through it
    for root, dirs, files in os.walk(dir): # Walk through each directory in 'dir'
        for file in files: # Look at each file in the directory we're examining now
            if file[-5:] == '.html': # We only care about html files in PASS 2; we already published everything else
                with open(os.path.join(root, file), 'r') as orig_html_file:
                    file_data = orig_html_file.read()

                    # Replace links to content we already published with CID lists
                    for key, value in cids_by_filename.iteritems():
                        # build the CID string to replace the filepath
                        cid_string = 'http://xia.cid.%i.' % len(value)
                        for cid in value:
                            cid_string += cid
                       
                        content_path  = os.path.relpath(key, root)
                        
                        # first match filepaths beginning with "./"
                        file_data = string.replace(file_data, './' + content_path, cid_string)
                        # now match filepaths without "./"
                        file_data = string.replace(file_data, content_path, cid_string)

                    # Replace links to other html files in 'dir' with this webserver's SID
                    for linked_html_file in html_files:
                        dag_url = 'http://dag/2,0/%s=0:2,1/%s=1:2/%s=2:2//%s' % (AD1, HID1, SID1, linked_html_file[5:])
                        
                        rel_path = os.path.relpath(linked_html_file, root)
                        # first match filepaths beginning with "./"
                        file_data = string.replace(file_data, './' + rel_path, dag_url)
                        # now match filepaths without "./"
                        file_data = string.replace(file_data, rel_path, dag_url)

                    # write modified html
                    with open(os.path.join(root, file+'TEMP'), 'w') as fnew:
                        fnew.write(file_data)
                    fnew.closed
                    
                    # publish the modified HTML file
                    cids_by_filename_to_add[os.path.join(root,file)] = put_file(os.path.join(root, file+'TEMP'), 1200)
                orig_html_file.closed
    cids_by_filename = dict(cids_by_filename.items() + cids_by_filename_to_add.items());


def main():
    # Set up connection with click via Xsocket API
    set_conf("xsockconf_python.ini", "webserver_replicate_geni.py")

    # TODO: When new put_chunk API is ready and we have persistent caching, we can eliminate
    # this and make a separate 'content publishing' app.
    try:
        sys.argv.index('-r') # don't republish content if we're restarting but didn't restart click
        print 'Restarting webserver. Don\'t republish content'
    except:
        put_content_in_dir('./www_replicate') # '-r' not found, so do publish content

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
                incoming_data = Xrecv(accept_sock, 2000, 0)
                serveHTTPRequest(incoming_data, accept_sock)
                Xclose(accept_sock)
                os._exit(0)
    except (KeyboardInterrupt, SystemExit), e:
       print 'Closing webserver'
       Xclose(listen_sock)
       sys.exit()
    


if __name__ ==  '__main__':
    main()
