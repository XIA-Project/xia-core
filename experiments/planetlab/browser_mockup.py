#!/usr/bin/python

import socket, time, re
PROXY_IP = '127.0.0.1'
PROXY_PORT = 8000
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


start = time.time()
data = get_file(FILE)

for r in re.finditer(r'href="(.*)"', data):
    get_file(r.group(1).strip())

for r in re.finditer(r'src="(.*)"', data):
    get_file(r.group(1).strip())

elapsed = (time.time() - start)
print elapsed
