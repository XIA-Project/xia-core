#!/usr/bin/python

import socket
import sys
import xia_pb2
import struct
import time

chunksize = 65536
CID = ['0000000000000000000000000000000000000000', '0000000000000000000000000000000000000001', '0000000000000000000000000000000000000010','0000000000000000000000000000000000000011','0000000000000000000000000000000000000100', '0000000000000000000000000000000000000101', '0000000000000000000000000000000000000110','0000000000000000000000000000000000000111','0000000000000000000000000000000000001000', '0000000000000000000000000000000000001001', '0000000000000000000000000000000000001010', '0000000000000000000000000000000000001011']
cid_i = 0
length = 0

def putCID(chunk, sock_rpc):
    global cid_i
    print cid_i
    msg_putCID = xia_pb2.msg()
    msg_putCID.type = xia_pb2.msg.PUTCID
    msg_putCID.xiapath_src = 'CID:' + CID[cid_i]  # todo: compute hash value
    msg_putCID.xiapath_dst = 'CID:' + CID[cid_i]
    cid_i = cid_i + 1
    msg_putCID.payload = chunk
    #msg_putCID.payload = "i'm a fake pic" + CID[cid_i]
    #print  msg_putCID.xiapath_dst, msg_putCID.payload
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
    msg_serveSID.payload = 'HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nLast-Modified: Sat, 08 Jan 2011 21:08:31 GMT\nCache-Control: no-cache\nAccept-Ranges: bytes\nContent-Length: ' + str(length) + '\nConnection: close\nContent-Type: text/html\n\n'+'CID:0000000000000000000000000000000000000000'
    serialized_msg = msg_serveSID.SerializeToString()
    size = struct.pack('!i', len(serialized_msg))
    sock_rpc.send(size)
    sock_rpc.send(serialized_msg)
    print "serveSID sent" + msg_serveSID.payload
    return

def main():
    sock_rpc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_rpc.connect(('',2001))
    sock_rpc.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1 )
    print 'connected'
    f = open ("index0.html", 'r')
    chunk = f.read(chunksize)
    global length
    length = len(chunk)
    print 'putcid'
    print 'cid len: %d' % len(chunk)
    putCID(chunk, sock_rpc)
    f.close()
    f = open ("image.jpg", 'r')
    while True:
        chunk = f.read(chunksize)
        if (chunk == ''):
            break;
        print 'putcid'
        print 'cid len: %d' % len(chunk)
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

