#!/usr/bin/python

import socket, time, re, sys
PROXY_IP = '127.0.0.1'
PROXY_PORT = 14659 #8000
BUFFER_SIZE = 65535

FILE = 'http://www_s.xiaweb.com.xia/xia/index.html'

HTTP_GET_BACK="""HTTP/1.1\n\raccept-language: en-US,en;q=0.5
accept-encoding: gzip, deflate
host: www_s.xiaweb.com.xia
accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
user-agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.8; rv:21.0) Gecko/20100101 Firefox/21.0
connection: close

"""


def create_http_get(file):
    return "%s %s %s" % ("GET", file, HTTP_GET_BACK)

def get_file(file):
    proxySocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    proxySocket.connect((PROXY_IP, PROXY_PORT))
    s = create_http_get(file)
    proxySocket.send(s)
    data = proxySocket.recv(BUFFER_SIZE)
    proxySocket.close()
    return data


if len(sys.argv) > 1:
    PROXY_PORT = int(sys.argv[1])

print 'starting browser test!!'
start = time.time()
data = get_file(FILE)
print 'browser data: %s' % data

rc = 42
for r in re.finditer(r'href="(\S*)"', data):
    print r.group(1).strip()
    get_file(r.group(1).strip())

for r in re.finditer(r'src="(\S*)"', data):
    print r.group(1).strip()
    get_file(r.group(1).strip())
    rc = 0

elapsed = (time.time() - start)
print 'Elapsed Time for Browser Test: %s' % elapsed
sys.exit(rc)
