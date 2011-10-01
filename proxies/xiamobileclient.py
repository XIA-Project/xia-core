#!/usr/bin/python

import socket, select, xia_pb2
import struct, time, signal, os

#CID = ['0000000000000000000000000000000000000000', '0000000000000000000000000000000000000001', '0000000000000000000000000000000000000010','0000000000000000000000000000000000000011','0000000000000000000000000000000000000100', '0000000000000000000000000000000000000101', '0000000000000000000000000000000000000110','0000000000000000000000000000000000000111','0000000000000000000000000000000000001000', '0000000000000000000000000000000000001001', '0000000000000000000000000000000000001010', '0000000000000000000000000000000000001011']
timestamps = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0]
stamp_i = 0
p_i = 0
local_addr = "AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000"


def timeout(CID, sock_rpc,  timeout_duration):
    ready = select.select([sock_rpc], [], [], timeout_duration)
    if ready[0]:
        data = sock_rpc.recv(4)
        size = struct.unpack('!i', data)[0]
	print "receive picture size", size
	data = sock_rpc.recv(size)
        msg_response = xia_pb2.msg()
	msg_response.ParseFromString(data) 
	payload =  msg_response.payload
	print 'here here here:' + str (len(payload))
	print "%.6f" % time.time()
	global stamp_i
	timestamps[stamp_i] = time.time()
	stamp_i = stamp_i + 1 
    else :
        print 'timeout:'
        #sock_rpc.send('timeout')
        payload = getCID (CID, sock_rpc, True)

    return payload


def read_write( soc_rpc, max_idling=20):
        global p_i
        iw = [soc_rpc]
        ow = []
        count = 0
	msg_response = xia_pb2.msg()
        while 1:
            count += 1
            (ins, _, exs) = select.select(iw, ow, iw, 3)
            if exs: break
            if ins:
                for i in ins:
                    data = i.recv(4)

                    size = struct.unpack('!i', data)[0]
		    data = i.recv(size)
		    
		    msg_response.ParseFromString(data) 
                    if data:
                        if (i == soc_rpc): #and (msg_response.appid == bsoc.fileno())):
                            print "Packet: " + str(p_i)
                            p_i += 1
                            print "payload len (recv): %d" % len(msg_response.payload)
                            print "payload (recv): %s" % msg_response.payload
                            SIDResponse = msg_response.payload
                            
            else:
                print "\t" "idle", count
            if count == max_idling: break
	    

def sendSIDRequest(netloc, payload,  sock_rpc):
        global local_addr
	print "in SID function - net location = " + netloc  
	msg = xia_pb2.msg()
	#msg.xid = '00000000000000000000'
	msg.type = xia_pb2.msg.CONNECTSID
	#payload = ''     # to be removed
        msg.payload = payload
        msg.xiapath_src = local_addr
        msg.xiapath_dst = 'AD:1000000000000000000000000000000000000001 HID:0000000000000000000000000000000000000001'  # sp SID: xxx
        serialized_msg = msg.SerializeToString()
	#print ''.join(map(lambda x: '%02x' % ord(x), serialized_msg))
        size = struct.pack('!i', len(serialized_msg))
        print "payload len %d " % len(payload)
        sock_rpc.send(size)
        sock_rpc.send(serialized_msg)
#	read_write(sock_rpc)
	
	return


def getCID(CID, sock_rpc, fallback):
	print "in getCID function"  
	print CID
	print "%.6f" % time.time()
	global stamp_i
	timestamps[stamp_i] = time.time()
	stamp_i = stamp_i + 1 
	msg = xia_pb2.msg()
	msg.type = xia_pb2.msg.GETCID
        msg.payload = ''
        msg.xiapath_src = 'AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000'
	if fallback == True :
		msg.xiapath_dst = '( AD:1000000000000000000000000000000000000001 HID:0000000000000000000000000000000000000001 ) CID:'+ CID
	else :
		msg.xiapath_dst = 'CID:'+ CID
        serialized_msg = msg.SerializeToString()
	#print ''.join(map(lambda x: '%02x' % ord(x), serialized_msg))
        size = struct.pack('!i', len(serialized_msg))
        #print "payload len %d " % len(payload)
	print "protobufmsg len %d " % len(serialized_msg)

        sock_rpc.send(size)
        sock_rpc.send(serialized_msg)
	#data = sock_rpc.recv(4)
	#size = struct.unpack('!i', data)[0]
	#print "receive picture size", size
	#data = sock_rpc.recv(size)
	data = timeout(CID, sock_rpc, 0.12)


	
	return data


def xiaHandler(control, payload,  sock_rpc):
	if payload.find('GET /favicon.ico') != -1:
			return
	print "in XIA code\n" + control + "\n" + payload
	control=control[4:]
	if control.find('sid') == 0:
		print "SID request"
		global stamp_i
#		timestamps[stamp_i] = time.time()
		stamp_i = stamp_i + 1 
                sendSIDRequest(control[4:], payload,  sock_rpc);
		
	return;


if __name__ == '__main__':
    global local_addr
    sock_rpc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	# Connect to rpc socket and send data
        #sock_rpc.setblocking(0)
    sock_rpc.connect(('', 2000)) #change: 80 -> 2000
    sock_rpc.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1 )
    for i in range(50000):
        payload = "PING:" + str(i)
        netloc = "xia.sid.connect"
        print "Hi"
        xiaHandler(netloc, payload, sock_rpc)


    local_addr = "AD:1000000000000000000000000000000000000011 HID:0000000000000000000000000000000000000000"
    
    payload = "UPDATE:: AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000 SID:3000000000000000000000000000000000000000; AD:1000000000000000000000000000000000000011 HID:0000000000000000000000000000000000000000 SID:3000000000000000000000000000000000000000"
    netloc = "xia.sid.updatesrcaddress"
   # xiaHandler(netloc, payload, sock_rpc)
