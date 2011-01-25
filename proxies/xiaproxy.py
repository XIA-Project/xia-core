import socket, select, xia_pb2
import struct, time

#CID = ['0000000000000000000000000000000000000000', '0000000000000000000000000000000000000001', '0000000000000000000000000000000000000010','0000000000000000000000000000000000000011','0000000000000000000000000000000000000100', '0000000000000000000000000000000000000101', '0000000000000000000000000000000000000110','0000000000000000000000000000000000000111','0000000000000000000000000000000000001000', '0000000000000000000000000000000000001001', '0000000000000000000000000000000000001010', '0000000000000000000000000000000000001011']
http_header = ''
timestamps = [0, 0, 0, 0, 0, 0]
stamp_i = 0

def read_write(bsoc, soc_rpc, max_idling=20):
        iw = [bsoc, soc_rpc]
        ow = []
        count = 0
	msg_response = xia_pb2.msg()
        while 1:
            count += 1
            (ins, _, exs) = select.select(iw, ow, iw, 3)
            if exs: break
            if ins:
                for i in ins:
                    if i is soc_rpc:
                        out = bsoc
                    else:
                        out = soc_rpc
                    data = i.recv(4)
		    print "sizesizesize: %d" % len(data)
                    size = struct.unpack('!i', data)[0]
		    data = i.recv(size)
		    
		    msg_response.ParseFromString(data) 
                    if data:
			    if (i == soc_rpc): #and (msg_response.appid == bsoc.fileno())):
				    print "payload len (recv): %d" % len(msg_response.payload)
				    #print "payload (recv): %s" % msg_response.payload
				    print len(msg_response.payload)
				    #payload_http = 'HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nLast-Modified: Sat, 08 Jan 2011 21:08:31 GMT\nCache-Control: no-cache\nAccept-Ranges: bytes\nContent-Length: ' + str(len(msg_response.payload)) +  '\nConnection: close\nContent-Type: text/html\n\n' + msg_response.payload + '\r\n\r\n'
				    global http_header
				    http_header = msg_response.payload
				    print "SID response: " + http_header
				    print "%.6f" % time.time()
				    global stamp_i
				    timestamps[stamp_i] = time.time()
				    stamp_i = stamp_i + 1 
				    payload = getCID ('0000000000000000000000000000000000000000', soc_rpc)
				    out.send(http_header + payload)  
			    count = 0
            else:
                print "\t" "idle", count
            if count == max_idling: break
	    

def sendSIDRequest(netloc, payload, bsoc, sock_rpc):
	print "in SID function - net location = " + netloc  
	msg = xia_pb2.msg()
	#msg.xid = '00000000000000000000'
	msg.type = xia_pb2.msg.CONNECTSID
	#payload = ''     # to be removed
        msg.payload = payload
        msg.xiapath_src = 'AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000'
        msg.xiapath_dst = 'AD:1000000000000000000000000000000000000001 HID:0000000000000000000000000000000000000001'  # sp SID: xxx
        serialized_msg = msg.SerializeToString()
	#print ''.join(map(lambda x: '%02x' % ord(x), serialized_msg))
        size = struct.pack('!i', len(serialized_msg))
        print "payload len %d " % len(payload)
        sock_rpc.send(size)
        sock_rpc.send(serialized_msg)
	read_write(bsoc, sock_rpc)
	
	return


def getCID(CID, sock_rpc):
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
        msg.xiapath_dst = '( AD:1000000000000000000000000000000000000001 HID:0000000000000000000000000000000000000001 ) CID:'+ CID
        serialized_msg = msg.SerializeToString()
	#print ''.join(map(lambda x: '%02x' % ord(x), serialized_msg))
        size = struct.pack('!i', len(serialized_msg))
        #print "payload len %d " % len(payload)
	print "protobufmsg len %d " % len(serialized_msg)

        sock_rpc.send(size)
        sock_rpc.send(serialized_msg)
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
	return payload


def xiaHandler(control, payload, bsock, sock_rpc):
	if payload.find('GET /favicon.ico') != -1:
			return
	print "in XIA code\n" + control + "\n" + payload
	control=control[4:]
	if control.find('sid') == 0:
		print "SID request"
		print "%.6f" % time.time()
		global stamp_i
		timestamps[stamp_i] = time.time()
		stamp_i = stamp_i + 1 
		if control.find('image.jpg') != -1:
			payload = 'image.jpg'
		sendSIDRequest(control[4:], payload, bsock, sock_rpc);
	elif control.find('cid') == 0:
		print "cid request"
		num = int(control[4])
		print "num %d" % num
		
		payload_cid = ''
		for i in range (0, num):
			print i
			payload_cid += getCID(control[6+i*40+i:46+i*40+i], sock_rpc)
			
		length = len (payload_cid)
		print length
		#header = 'HTTP/1.1 200 OK\nETag: "48917-39ed-4990ddae564c"\nAccept-Ranges: bytes\nContent-Length: ' + str(length) + '\nContent-Type: image/jpeg\n\n'  # todo: avoid hard code 
		#header = 'HTTP/1.1 200 OK\nAccept-Ranges: bytes\nCache-Control: no-cache\nContent-Length: 14829\nContent-Type: image/jpeg\n\n'  # todo: avoid hard code 
		header2 = ''
		payload_http =  payload_cid
		bsock.send(payload_http)
		for i in range (1, 6):
			interval = timestamps[i] - timestamps[i-1]
			print "%.6f" % interval
		interval = timestamps[5] - timestamps[0]
		print "Completion time: %.6f" % interval
		global stamp_i
		stamp_i = 0
	elif control.find('socket') == 0:
		#sendSIDRequest(control[7:], payload, bsock, sock_rpc);
		sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		sock.connect(('', 80))
		sock.send(payload)
		read_write(bsock, sock)
		
	return;

