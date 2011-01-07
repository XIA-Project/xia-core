import socket
import sys
import xia_pb2
import struct


def putCID(chunk, sock_rpc):
    msg_putCID = xia_pb2.msg()
    msg_putCID.type = xia_pb2.msg.PUTCID
    msg_putCID.xiapath_src = 'CID:0000000000000000000000000000000000000000'
    msg_putCID.xiapath_dst = 'CID:0000000000000000000000000000000000000000'
    msg_putCID.payload = chunk
    print chunk
    serialized_msg = msg_putCID.SerializeToString()
    size = struct.pack('!i', len(serialized_msg))
    sock_rpc.send(size)
    sock_rpc.send(serialized_msg)
    return

def serveSIDRequest(msg_protobuf):
    msg_response = xia_pb2.msg()
    #msg.destpath = '00000000000000000000'
    #msg.type = xia_pb2.msg.serveSIDRequest
    #readchunk
    msg_response.xiapath_src = msg_protobuf.xiapath_dst
    msg_response.xiapath_dst = msg_protobuf.xiapath_src
    f = open ("index.html", 'r')
    payload = ''
    while True:
        chunk = f.read(8096)
        if (chunk == ''):
            break;
        payload += chunk
        return
    print "payload: %s" % payload
    msg_response.payload = payload
#    msg_response.type = xia_pb2.serveSIDRequest
    sock.send(msg_response.SerializeToString())
    return

def main():
    chunksize = 8096
    sock_rpc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_rpc.connect(('',2001))
    print 'connected'
    f = open ("fake", 'r')
    while True:
        chunk = f.read(chunksize)
        if (chunk == ''):
            break;
        putCID(chunk, sock_rpc)
        return
    print "putcid done"
    #size = sock.recv(4)
    #size = struct.unpack('!i', size)[0]
    #print "size of protobuf msg: %d" % size
    #data = sock.recv(size)
    #msg_protobuf = xia_pb2.msg()
    #msg_protobuf.ParseFromString(data)
    # check if connectSID
    #serveSIDRequest(msg_protobuf)
    #putCID(chunk)
	
   

    sock.close()


if __name__ ==  '__main__':
    main()
