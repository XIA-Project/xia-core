#!/usr/bin/python
## A simple video proxy -- testing for video XIA server
## would need to change it to a more generic proxy, 
## or merge its changes with the generic proxy

"exec" "python" "-O" "$0" "$@"

__doc__ = """Tiny HTTP Proxy.

This module implements GET, HEAD, POST, PUT and DELETE methods
on BaseHTTPServer, and behaves as an HTTP proxy.  The CONNECT
method is also implemented experimentally, but has not been
tested yet.

Any help will be greatly appreciated.		SUZUKI Hisao
"""

__version__ = "0.2.1"

import BaseHTTPServer, select, socket, SocketServer, urlparse, string
import struct
import xsocket
import os
import io
import sys
from ctypes import *

HID0= "HID:0000000000000000000000000000000000000000"
HID1= "HID:0000000000000000000000000000000000000001"
AD0=  "AD:1000000000000000000000000000000000000000"
AD1=  "AD:1000000000000000000000000000000000000001"
RHID0="HID:0000000000000000000000000000000000000002"
RHID1="HID:0000000000000000000000000000000000000003"
CID0= "CID:2000000000000000000000000000000000000001"
SID0= "SID:0f00000000000000000000000000000000000055"

class ProxyHandler (BaseHTTPServer.BaseHTTPRequestHandler):
    __base = BaseHTTPServer.BaseHTTPRequestHandler
    __base_handle = __base.handle

    server_version = "TinyHTTPProxy/" + __version__
    rbufsize = 0                        # self.rfile Be unbuffered

    def handle(self):
        (ip, port) =  self.client_address
        if hasattr(self, 'allowed_clients') and ip not in self.allowed_clients:
            self.raw_requestline = self.rfile.readline()
            if self.parse_request(): self.send_error(403)
        else:
            self.__base_handle()

    def _connect_to(self, netloc, soc):
        i = netloc.find(':')
        if i >= 0:
            host_port = netloc[:i], int(netloc[i+1:])
        else:
            host_port = netloc, 80
        print "\t" "connect to %s:%d" % host_port
        try: soc.connect(host_port)
        except socket.error, arg:
            try: msg = arg[1]
            except: msg = arg
            self.send_error(404, msg);
            return 0
        return 1

    def do_CONNECT(self):
        soc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            if self._connect_to(self.path, soc):
                self.log_request(200)
                self.wfile.write(self.protocol_version +
                                 " 200 Connection established\r\n")
                self.wfile.write("Proxy-agent: %s\r\n" % self.version_string())
                self.wfile.write("\r\n")
                self._read_write(soc, 300)
        finally:
            print "\t" "bye"
            soc.close()
            self.connection.close()

    def do_GET(self):

	xsocket.set_conf("xsockconf_python.ini","videoproxy.py")
	xsocket.print_conf()
        print "Get request:"
        (scm, netloc, path, params, query, fragment) = urlparse.urlparse(
            self.path, 'http')
	print "netloc=" + netloc
	if netloc.find('xia') == 0:
		header = "%s %s %s\r\n" % (
                    self.command,
                    urlparse.urlunparse(('', '', path, params, query, '')),
                    self.request_version)
		self.headers['Connection'] = 'close'
		del self.headers['Proxy-Connection']
		for key_val in self.headers.items():
			header+="%s: %s\r\n" % key_val

		header+="\r\n"

		## hack for XIA		
		xsocket.set_conf("xsockconf_python.ini","videoproxy.py")
		xsocket.print_conf()

		sock=xsocket.Xsocket()

		if (sock<0):
			print "error opening socket"
			exit(-1)

		sock1=xsocket.Xsocket()

		if (sock1<0):
			print "error opening socket"
			exit(-1)

		dag = "RE %s %s %s" % (AD0, HID0, SID0)
		xsocket.Xconnect(sock,dag);  

		payload = "hello world"
		xsocket.Xsend(sock, payload, len(payload),0)

		reply = xsocket.Xrecv(sock, 1024, 0)
		numcids = int(reply)

		print "number of cids",numcids

		self.send_response(200)
                self.send_header('Content-type',	'video/ogg')
		self.end_headers()

		#imgfile = open('ofile', 'wb')

		testing = "echo"
		cidliststr = ""
		for i in range(numcids):
			xsocket.Xsend(sock,testing, len(testing), 0);
			reply = xsocket.Xrecv(sock, 1024, 0)
			cid = reply
			print "trying ",cid
			dag = "RE %s %s %s" % (AD0, HID0, cid)
			xsocket.XgetCID(sock1, dag, len(dag))
			reply = xsocket.Xrecv(sock1, 8192, 0)
			#imgfile.write(reply)
			self.connection.send(reply)

                #imgfile.close()
		return
	
        if scm != 'http' or fragment or not netloc:
            self.send_error(400, "bad url %s" % self.path)
            return
        soc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            if self._connect_to(netloc, soc):
                self.log_request()
                soc.send("%s %s %s\r\n" % (
                    self.command,
                    urlparse.urlunparse(('', '', path, params, query, '')),
                    self.request_version))
                self.headers['Connection'] = 'close'
                del self.headers['Proxy-Connection']
                for key_val in self.headers.items():
						print("%s: %s\r\n" % key_val)
						soc.send("%s: %s\r\n" % key_val)
                soc.send("\r\n")
                self._read_write(soc)
					
        finally:
            print "\t" "bye"
            soc.close()
            self.connection.close()

    def _read_write(self, soc, max_idling=20):
        iw = [self.connection, soc]
        ow = []
        count = 0
        while 1:
            count += 1
            (ins, _, exs) = select.select(iw, ow, iw, 3)
            if exs: break
            if ins:
                for i in ins:
                    if i is soc:
                        out = self.connection
                    else:
                        out = soc
                    data = i.recv(8192)
                    if data:
                        print data
                        out.send(data)
                        count = 0
            else:
                print "\t" "idle", count
            if count == max_idling: break

    do_HEAD = do_GET
    do_POST = do_GET
    do_PUT  = do_GET
    do_DELETE=do_GET

class ThreadingHTTPServer (SocketServer.ThreadingMixIn,
                           BaseHTTPServer.HTTPServer): pass

if __name__ == '__main__':
    from sys import argv
    if argv[1:] and argv[1] in ('-h', '--help'):
        print argv[0], "[port [allowed_client_name ...]]"
    else:
        if argv[2:]:
            allowed = []
            for name in argv[2:]:
                client = socket.gethostbyname(name)
                allowed.append(client)
                print "Accept: %s (%s)" % (client, name)
            ProxyHandler.allowed_clients = allowed
            del argv[2:]
        else:
            print "Any clients will be served..."
	sock_rpc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        BaseHTTPServer.test(ProxyHandler, ThreadingHTTPServer)
