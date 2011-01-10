#!/usr/bin/python

import socket
import sys
import xia_pb2
import struct
import time

chunksize = 65536
#CID[chunk]

def putCID(chunk, sock_rpc):
    msg_putCID = xia_pb2.msg()
    msg_putCID.type = xia_pb2.msg.PUTCID
    msg_putCID.xiapath_src = 'CID:0000000000000000000000000000000000000000' # todo: compute hash value
    msg_putCID.xiapath_dst = 'CID:0000000000000000000000000000000000000000'
    chunk_http ='HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nLast-Modified: Sat, 08 Jan 2011 21:08:31 GMT\nETag: "48917-39ed-4990ddae564ac"\nAccept-Ranges: bytes\nContent-Length: 14829\nConnection: close\nContent-Type: image/jpeg\n\n' +chunk + '\r\n\r\n'
    msg_putCID.payload = chunk_http
    print chunk
    serialized_msg = msg_putCID.SerializeToString()
    size = struct.pack('!i', len(serialized_msg))
    sock_rpc.send(size)
    sock_rpc.send(serialized_msg)
    return

def serveSIDRequest(msg_protobuf, sock_rpc):
    msg_serveSID = xia_pb2.msg()
    msg_serveSID.type = xia_pb2.msg.SERVESID
    msg_serveSID.xiapath_src = msg_protobuf.xiapath_dst
    msg_serveSID.xiapath_dst = msg_protobuf.xiapath_src
    f = open ("index.html", 'r')
    #sock_apache = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    #sock_apache.connect(('127.0.0.1', 80))
    

    #sock_apache.close()
    payload = f.read(chunksize)
    #while True:
    #    chunk = f.read(chunksize)
    #    if (chunk == ''):
    #        break;
    #    print chunk
    #    payload += chunk
    #    return
    payload_http = 'HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nLast-Modified: Sat, 08 Jan 2011 21:08:31 GMT\nETag: "43127-76-4995c231bf691"\nAccept-Ranges: bytes\nContent-Length: 118\nConnection: close\nContent-Type: text/html\n\n' + payload + '\r\n\r\n'    #add http header
    print "payload:" + payload_http
    msg_serveSID.payload = payload_http
    serialized_msg = msg_serveSID.SerializeToString()
    size = struct.pack('!i', len(serialized_msg))
    sock_rpc.send(size)
    sock_rpc.send(serialized_msg)
    print "serveSID sent"
    f.close()
    return

def main():
    sock_rpc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_rpc.connect(('',2001))
    print 'connected'
    f = open ("image.jpg", 'r')
    while True:
        chunk = f.read(chunksize)
        if (chunk == ''):
            break;
        putCID(chunk, sock_rpc)
#       return
    print "putcid done"
    f.close()
    
    
    time.sleep(1)
    while True:        
        size = sock_rpc.recv(4)
        size = struct.unpack('!i', size)[0]
        print "size of protobuf msg: %d" % size
        data = sock_rpc.recv(size)
        msg_request = xia_pb2.msg()
        msg_request.ParseFromString(data)
        if (msg_request.type == xia_pb2.msg.CONNECTSID):
            print "SIDRequest comes"
        serveSIDRequest(msg_request, sock_rpc)


    
    #sock_rpc.recv(4)	
   

    sock_rpc.close()


if __name__ ==  '__main__':
    main()
