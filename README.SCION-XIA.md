
#this README describes the insturctions to run integrated SCION-XIA demo, some details you may need to know, known issues, and future work.

#version: 1.0
#Date: 03/10/2016

## Instructions and details

### 1. go to scion git repository, and start to run SCION infrastructure and SCION daemon

### 1.1 checkout xia-scion-yanlin branch.
   
`git checkout xia-scion-yanlin` (Yanlin is still asking ETH folks to help create this branch. Please checkou the main branch, and then ask Yanlin for the topology configure file SCIONXIA.topo) 

Please note the main change we made to SCION source code are (1) we added a new topology configuration file (topology/SCIONXIA.topo), (2) we added more debugging messages.

### 1.2 start to run SCION infrastructure with the simple topology

`cp topology/SCIONXIA.topo topology/Default.topo`

Following the instructions in README.md to run SCION infrastructure


### 1.3 run SCION daemon (SCION offers the SCION daemon for SCION hosts to get path information. One application-layer scion gateway in the integrated SCION-XIA system will create a connection with the SCION daemon, and then obtain path information through the routing path information). 

`./scion sock_cli 1 12` (in this command, 1 is the ISD id while 12 is the AD id. The scion gateway that connects to this SCION daemon will be in the same AD with this SCION daemon).


### 2 go to xia repository to run xia infrastructure

### 2.1 checkout xia-scion-yanlin branch.

`git checkout xia-scion-yanlin`

### 2.2 build xia system

`make clean`

`./configure`

`make`

### 2.3 run xia

`./bin/xianet -vl7 start`

### 2.4 add the SID of scion gateway in etc/resolv.conf

`grep xgate /var/log/syslog`

the SID of scion_gateway is the second SID to the last in the output. In etc/resolv.conf, add the follwing line:
scion_gateway=SID:xxxxxxxxxxxxxxxxxxxxxxxxxx 

For example, following is one output of `grep xgate /var/log/syslog`:

Mar 10 14:05:33 yanlin xgateway: No SID provided. Creating a new one
Mar 10 14:05:33 yanlin xgateway: New service ID:SID:60e7ae56df76ed8eeafa81cb2032223df94cdf05: created
Mar 10 14:05:33 yanlin xgateway: No SID provided. Creating a new one
Mar 10 14:05:33 yanlin xgateway: New service ID:SID:0c3cf9d2235f268c25fcaa9b208aed2d1a18246f: created
Mar 10 14:05:33 yanlin xgateway: DEBUG: create SID is done
Mar 10 14:05:34 yanlin xgateway: No SID provided. Creating a new one
Mar 10 14:05:34 yanlin xgateway: New service ID:SID:c9399ed5439e3985e6255b0c285f2ae41458483e: created
Mar 10 14:05:34 yanlin xgateway: No SID provided. Creating a new one
Mar 10 14:05:34 yanlin xgateway: New service ID:SID:8e37c2e8435abcf54c1d3425a1b6a983e29e1fb6: created
Mar 10 14:05:34 yanlin xgateway: DEBUG: create SID is done

we copy `SID:c9399ed5439e3985e6255b0c285f2ae41458483e` to resolv.conf, and add the following line to resolv.conf:

scion_gate=SID:c9399ed5439e3985e6255b0c285f2ae41458483e

Details: now the scion gateway (source code is in daemon/gateway) creates SIDs for itself, and we need to manually add its SID in resolv.conf. Then the transport layer code can read the resolv.conf to get the SID, and add scion gateway's scion to the XIA header, guaranteeing that the packet from client host will be forwarded to scion gateway first.

### 2.5 start server for the demo

`cd applications/example`

`./echoserver`

You shoud see following messages from server"

"""""""
XIA Echo Server (v1.0): started
Datagram service started

Datagram DAG
DAG 0 - 
AD:39612e3e7388e9c4b7a60cff4b900f529000d3e5 1 - 
HID:bf3aadc320a844335bf2b6d34a3f94372699cc15 2 - 
SID:0f00000000000000000000000000000000008888
registered name: 
www_s.dgram_echo.aaa.xia
Stream service started
Dgram Server waiting
XreadRVServerAddr: reading config file:/home/yanlin/github/xia-core/etc/resolv.conf:
XreadRVServerAddr: Rendezvous server DAG not found::

Stream DAG
DAG 0 - 
AD:39612e3e7388e9c4b7a60cff4b900f529000d3e5 1 - 
HID:bf3aadc320a844335bf2b6d34a3f94372699cc15 2 - 
SID:6e792f7d1a85b7fad569043e776ae70318ad7f28
registered name: 
www_s.stream_echo.aaa.xia
Xbind: Intent:SID:6e792f7d1a85b7fad569043e776ae70318ad7f28:
Xsock    4 waiting for a new connection.
Xsock    5 new session
peer:DAG 0 - 
AD:0ab57131f477b8b8309c1c84484e108077a72378 1 - 
HID:4a6a74055bc752eb6ceb3f8203dc17932cc3ab9a 2 - 
SID:9cb568f44b7dd0216182c0292a16b798ad8b6395
Xsock    4 waiting for a new connection.
"""""""


### 2.6 start client for the demo

`./echoclient`

If the demo works, you can see the following messages from client:

""""
XIA Echo Client (v1.0): started

DAG 0 - 
SID:3b45d8227185ede5c5614d86f65fc153ea939782 1 - 
AD:39612e3e7388e9c4b7a60cff4b900f529000d3e5 2 - 
HID:bf3aadc320a844335bf2b6d34a3f94372699cc15 3 - 
SID:6e792f7d1a85b7fad569043e776ae70318ad7f28
Xsock    3 created
Xsock    3 connected
Xsock    3 sent 512 of 512 bytes
Xsock    3 received 512 bytes
Xsock    3 closed
"""""

You should see following message from server:

""""
 5081 received 512 bytes
 5081 sent 512 bytes
5081 client closed the connection
 5081 closing
""""

You can also see messages from XIA:

"""""""
46 bytes response from daemon
path length is 24 bytes
Print scion path info, path length 24 bytes:
InfoOpaqueField:
info 40
flag 1
dump contents:
81 56 e1 d3 b2 0 1 2 
info 0x40, flag 1, timestamp 0, isd-id 1, hops 2
dump contents:
0 3f 0 20 0 7d f ed 
Ingress 2, Egress 0
dump contents:
10 3f 0 0 1 fd 5e 8b 
Ingress 0, Egress 1
"""""""


Details: the echoclient (source code applications/example/echoclient.c) sets a special flag through "hints.ai_flags = XAI_SCION" to inform the transport layer that it is going to use XIA-SCION network (line 331 in echoclient.c). Then transport layer will add the SID of the scion gateway in the DAG for all packets from the echo client, to make sure all packets from the echo client will be forwarded to the application-layer scion gateway first, and then the scion gateway will (1) send a path requst to the SCION daemon to get the SCION forwarding path information, and (2) add SCION header and path as an extension header in XIA header.


## Known issues

(1) On some virtual machines, we found that XIA is not stable. After we run the XIA infrastructure, we cannot even run xroute to check the route information. Thus, after running XIA infrastructure, wait a minute, and the run `./bin/xroute` to check if XIA is running correctly. If not, you may stop XIA (`./bin/xianet stop`), and restart XIA. You may also try to restart the virtual machine, and run the system again.


## Future work

(1) In the current demo, only the packets from client to server are going through scion-xia forwarding engines. The reverse packets (server to client) go through XIA forwarding engines. As future work, we should update the system to process the reverse packets through scion-xia forwarding engines. One possible way to implement it is .................

(2) In the current demo, the system only supports one router in each AD without local routing (am I correct?). However, SCION routing needs to know if the packets are from a local socket (the same AD) or now. Because of the limination in current XIA branch, we only use egress interface for scion-xia routing in the scion-xia forwarding engine (source code is in click-2.0.1/element/xia/xianewroutetable.cc lines 1101 to 1114). 




