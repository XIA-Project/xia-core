#!/usr/bin/python
import sys
import os
                                                                                                                                                                                                                    
# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

import math
import socket
import struct
import time
import datetime
import string
from c_xsocket import *
from ctypes import *
import hashlib
from xia_address import *

cids_by_filename = {}
myAD, my4ID, myHID, mySID = ('', '', '', '')

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
    content_length = 250
    http_header = 'Date: %s\nServer: XIA Baby Webserver\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type: text/html\nX-Length: %d\n\n' % (date, content_length)

    # If file exists in cids_by_filename, return its CID (list); otherwise return 404 Not Found
    requested_file = './www/' + request.split(' ')[1][1:]
    print 'requested file: %s' % requested_file
    try:
        cid_list = cids_by_filename[requested_file]
        cid_string = 'cid.%i.' % len(cid_list)
        for cid_info in cid_list:
            cid_string += cid_info.cid

        response_data = cid_string
        http_msg_type = 'HTTP/1.1 200 OK\n'
    except KeyError:
        print 'WARNING: webserver.py: serverHTTPRequest: Could not find requested file: %s' % requested_file
        response_data = '<html><body><h1>Sorry, we can\'t find that page.</h1></body></html>'
        http_msg_type = "HTTP/1.1 404 Not Found\n"

    # Send response
    content_length = len(response_data)
    http_header = 'Date: %s\nServer: XIA Baby Webserver\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type: text/html\nX-Length: %d\n\n' % (date,content_length)

    response = http_msg_type + http_header + response_data
    last_sent_length = 0;
    while (content_length + len(http_msg_type) + len(http_header)) > last_sent_length:
        base = last_sent_length
        last_sent_length = min(len(response), last_sent_length + 800)
        Xsend(sock, response[base:last_sent_length], 0)

# Chunk and publish all files in the local www directory.
# If the file is an html file and contains images also stored in
# the local www directory, publish those images first and replace
# the images' URLs in the web chunks we publish with the list of
# CIDs we just made
def put_content_in_dir(dir):
    global cids_by_filename
    if not os.path.exists(dir):
        print 'ERROR: webserver.py: put_content_in_dir: Directory "%s" does not exist.' % dir
        return

    # Allocate a local cache slice for the webserver
    chunk_context = XallocCacheSlice(POLICY_DEFAULT, 0, 0)

    # All files with extentions listed below won't be processed on the first pass;
    # they will be processed in successive passes in the order listed
    link_files_type_order = ['.css', '.html']

    # PASS 1:
    # publish each file in the given directory whose type does not appear in 
    # link_files_type_order; keep a dictionary mapping filepath to the corresponding
    # list of CIDs
    cids_by_filename = {}
    files_with_links = []
    for root, dirs, files in os.walk(dir):
        for file in files:
            if link_files_type_order.count(os.path.splitext(file)[1]) == 0 and link_files_type_order.count(os.path.splitext(file)[1][:-4]) == 0:
                cids_by_filename[os.path.join(root,file)] = XputFile(chunk_context, os.path.join(root, file), 1000)
            else:
                files_with_links.append(os.path.join(root, file))

    # PASSES 2 through N:
    # Process files that might contain links to other files one type at a time.
    # find references to any of the files we have already
    # published as content. Replace the reference with the corresponding
    # CID list. Replace references to other local html files with DAG
    # pointing to this webserver. Then publish the modified file. 
    for file_type in link_files_type_order:
        cids_by_filename_to_add = {} # need a temp dict here because we can't add to cids_by_filename while we're iterating through it
        for root, dirs, files in os.walk(dir): # Walk through each directory in 'dir'
            for file in files: # Look at each file in the directory we're examining now
                if os.path.splitext(file)[1] == file_type: # We only care about files of type file_type in this pass
                    with open(os.path.join(root, file), 'r') as orig_html_file:
                        file_data = orig_html_file.read()

                        # Replace links to content we already published with CID lists
                        for key, value in cids_by_filename.iteritems():
                            # build the CID string to replace the filepath
                            cid_string = 'xia.cid.%i.%s.%s.%s.' % (len(value), myAD, my4ID, myHID)
                            cid_string = cid_string.replace(':', '.')
                            cid_string = "http://%s" % cid_string
                            for cid_info in value:
                                cid_string += cid_info.cid
                           
                            content_path  = os.path.relpath(key, root)
                            
                            # first match filepaths beginning with "./"
                            file_data = string.replace(file_data, './' + content_path, cid_string)
                            # now match filepaths without "./"
                            file_data = string.replace(file_data, content_path, cid_string)

                        # Replace links to other html files in 'dir' with this webserver's SID
                        for linked_html_file in files_with_links:
                            dag_url = 'http://dag/3,0,1/%s=0:3,2/%s=1:3,0/%s=2:3/%s=3:3//%s' % (myAD, my4ID, myHID, mySID, linked_html_file[5:])
                            
                            rel_path = os.path.relpath(linked_html_file, root)
                            # first match filepaths beginning with "./"
                            file_data = string.replace(file_data, '"./' + rel_path + '"', '"' + dag_url + '"')
                            # now match filepaths without "./"
                            file_data = string.replace(file_data, '"' + rel_path + '"', '"' + dag_url + '"')

                        # write modified html
                        with open(os.path.join(root, file+'TEMP'), 'w') as fnew:
                            fnew.write(file_data)
                        fnew.closed
                        
                        # publish the modified HTML file
                        cids_by_filename_to_add[os.path.join(root,file)] = XputFile(chunk_context, os.path.join(root, file+'TEMP'), 1000)
                        os.remove(os.path.join(root, file+'TEMP'))
                    orig_html_file.closed
        cids_by_filename = dict(cids_by_filename.items() + cids_by_filename_to_add.items());


def main():
    global myAD, myHID, mySID, my4ID
        
    try:   
        # Create socket for listening for connections
        listen_sock = Xsocket(XSOCK_STREAM)
        if (listen_sock<0):
            print 'webserver.py: main: error opening socket'
            return

        # Get local AD, HID, and 4ID; build DAG to listen on
        (myAD, myHID, my4ID) = XreadLocalHostAddr(listen_sock)
        mySID = XmakeNewSID()

        # TODO: When we have persistent caching, we can eliminate
    	# this and make a separate 'content publishing' app.
    	put_content_in_dir('./www')         
        
        print '4ID: %s' % my4ID
        listen_dag_re = "RE %s %s %s" % (myAD, myHID, mySID) # dag to listen on; TODO: fix Xbind so this can be DAG format, not just RE
        listen_dag = "DAG 3 0 1 - \n %s 3 2 - \n %s 3 0 - \n %s 3 - \n %s" % (myAD, my4ID, myHID, mySID)

#        Xbind(listen_sock, listen_dag_re)
        Xbind(listen_sock, listen_dag)
        print 'Webserver listening on %s' % listen_dag

        Xlisten(listen_sock, 5)

        # Publish DAG to naming service
        XregisterName("www_s.xiaweb.com.xia", listen_dag)
        print 'registered name'

	# This is just for web demo.... publishing a CID
        imageCID = "CID:aa2fe45640e694c06dcc3331ed91998c8eb4879a"
        image_dag = "DAG 3 0 1 - \n %s 3 2 - \n %s 3 0 - \n %s 3 - \n %s" % (myAD, my4ID, myHID, imageCID)     
        XregisterName("www_c.airplane.com.xia", image_dag) 
       
        # TODO: use threads instead of processes?
        while(True):
            (accept_sock, peer) = Xaccept(listen_sock);
            print 'connection accepted'
            child_pid = os.fork()
  
            if child_pid == 0:  
                incoming_data = Xrecv(accept_sock, 2000, 0)
                serveHTTPRequest(incoming_data, accept_sock)
                Xclose(accept_sock)
                os._exit(0)
    except (KeyboardInterrupt, SystemExit), e:
       print 'Closing webserver'
       Xclose(listen_sock)
       XremoveSID(mySID)
       sys.exit()
    


if __name__ ==  '__main__':
    main()
