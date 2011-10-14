import socket, select, xia_pb2
import struct, time, signal, os

#CID = ['0000000000000000000000000000000000000000', '0000000000000000000000000000000000000001', '0000000000000000000000000000000000000010','0000000000000000000000000000000000000011','0000000000000000000000000000000000000100', '0000000000000000000000000000000000000101', '0000000000000000000000000000000000000110','0000000000000000000000000000000000000111','0000000000000000000000000000000000001000', '0000000000000000000000000000000000001001', '0000000000000000000000000000000000001010', '0000000000000000000000000000000000001011']
timestamps = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
stamp_i = 0


def timeout(CID, sock_rpc,  timeout_duration):
    ready = select.select([sock_rpc], [], [], timeout_duration)
    payload = None
    if ready[0]:
        data = sock_rpc.recv(4)
        size = struct.unpack('!i', data)[0]
        print "receive picture size", size
        data = sock_rpc.recv(size)
        print "receive %d" % len(data)
        msg_response = xia_pb2.msg()
        msg_response.ParseFromString(data) 
        payload =  msg_response.payload
        print 'here here here:' + str (len(payload))
        print "%.6f" % time.time()
        global stamp_i
        timestamps[stamp_i] = time.time()
        stamp_i = stamp_i + 1 
    elif CID!=None:
        print 'timeout:'
        #sock_rpc.send('timeout')
        payload = getCID (CID, sock_rpc, True)

    return payload


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
                if (len(data)!=4):
                        continue
                size = struct.unpack('!i', data)[0]
                data = i.recv(size)
                
                msg_response.ParseFromString(data) 
                if data:
                    if (i == soc_rpc): #and (msg_response.appid == bsoc.fileno())):
                        print "payload len (recv): %d" % len(msg_response.payload)
                        #print "payload (recv): %s" % msg_response.payload
                        print len(msg_response.payload)
                        SIDResponse = msg_response.payload
                        print "%.6f" % time.time()
                        global stamp_i
                        timestamps[stamp_i] = time.time()
                        stamp_i = stamp_i + 1 
                        # distinguish modes 1 and 2
                        rt = msg_response.payload.find('CID') 
                        print rt
                        if (rt!= -1):
                            http_header = SIDResponse[0:rt]
                            #print '!!'+ http_header
                            #print SIDResponse[rt+4:rt+44]
                            requestCID(SIDResponse[rt+4:rt+44], soc_rpc, True)
                            cnt=1
                            while ((cnt+1)*44 <=len(SIDResponse)-rt):
                                requestCID (SIDResponse[rt+44+4:rt+44+44], soc_rpc, True)
                                cnt+=1
                            payload = timeout(None, soc_rpc, 1.0)
                            for i in range(0,cnt-1):
                                timeout(None, soc_rpc, 1.0)

                            out.send (http_header+payload)
                        else:
                            out.send(SIDResponse)  
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

def requestCID(CID, sock_rpc, fallback):
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
    print msg.xiapath_dst
    print "protobufmsg len %d " % len(serialized_msg)

    sock_rpc.send(size)
    sock_rpc.send(serialized_msg)
    #data = sock_rpc.recv(4)
    #size = struct.unpack('!i', data)[0]
    #print "receive picture size", size
    #data = sock_rpc.recv(size)

    

def getCID(CID, sock_rpc, fallback):
    requestCID(CID, sock_rpc, fallback)
    data = timeout(CID, sock_rpc, 0.12)
    return data


def xiaHandler(control, payload, bsock, sock_rpc):
    if payload.find('GET /favicon.ico') != -1:
                    return
    print "in XIA code\n" + control + "\n" + payload
    control=control[4:]  # remove the 'xia.' prefix
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
            payload_cid += getCID(control[6+i*40+i:46+i*40+i], sock_rpc, False)
            
        length = len (payload_cid)
        print "payload_cid length %d " % length
        #header = 'HTTP/1.1 200 OK\nETag: "48917-39ed-4990ddae564c"\nAccept-Ranges: bytes\nContent-Length: ' + str(length) + '\nContent-Type: image/jpeg\n\n'  # todo: avoid hard code 
        #header = 'HTTP/1.1 200 OK\nAccept-Ranges: bytes\nCache-Control: no-cache\nContent-Length: 14829\nContent-Type: image/jpeg\n\n'  # todo: avoid hard code 
        header2 = ''
        payload_http =  payload_cid
        bsock.send(payload_http)
        for i in range (0, stamp_i):
            print "%.6f" % timestamps[i]
        for i in range (1, stamp_i):
            interval = timestamps[i] - timestamps[i-1]
            print "%.6f" % interval
        interval = timestamps[stamp_i-1] - timestamps[0]
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

