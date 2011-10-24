elementclass GenericRouting4Port {
    // input: a packet to route
    // output[0]: forward to port 0~3 (painted)
    // output[1]: need to update "last" pointer
    // output[2]: no match

    input -> rt :: XIAXIDRouteTable;
    rt[0] -> Paint(0) -> [0]output;
    rt[1] -> Paint(1) -> [0]output;
    rt[2] -> Paint(2) -> [0]output;
    rt[3] -> Paint(3) -> [0]output;
    rt[4] -> [1]output;
    rt[5] -> [2]output;
};

elementclass GenericPostRouteProc {
    input -> XIADecHLIM -> output;
};

elementclass XIAPacketRoute {
    $local_addr, $ip_addr |

    // $local_addr: the full address of the node (only used for debugging)

    // input: a packet to process
    // output[0]: forward (painted)
    // output[1]: arrived at destination node
    // output[2]: could not route at all (tried all paths)

    check_dest :: XIACheckDest();
    consider_first_path :: XIASelectPath(first);
    consider_next_path :: XIASelectPath(next);
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, next IP, -);

    input -> Print("packet received by $local_addr") -> consider_first_path;
    //input -> consider_first_path;

    check_dest[0] -> [1]output;             // arrived at the final destination
    check_dest[1] -> consider_first_path;   // reiterate paths with new last pointer

    consider_first_path[0] -> c;
    consider_first_path[1] -> [2]output;
    consider_next_path[0] -> c;
    consider_next_path[1] -> [2]output;

    //  Next destination is AD
    c[0] -> rt_AD :: GenericRouting4Port;
    rt_AD[0] -> GenericPostRouteProc -> [0]output;
    rt_AD[1] -> XIANextHop -> check_dest;
    rt_AD[2] -> consider_next_path;

    //  Next destination is HID
    c[1] -> rt_HID :: GenericRouting4Port;
    rt_HID[0] -> GenericPostRouteProc -> [0]output;
    rt_HID[1] -> XIANextHop -> check_dest;
    rt_HID[2] -> consider_next_path;

    //  Next destination is SID
    c[2] -> rt_SID :: GenericRouting4Port;
    rt_SID[0] -> GenericPostRouteProc -> [0]output;
    rt_SID[1] -> XIANextHop -> check_dest;
    rt_SID[2] -> consider_next_path;


    // change this if you want to do CID post route processing for any reason
    CIDPostRouteProc :: Null;

    //  Next destination is CID
    c[3] -> rt_CID :: GenericRouting4Port;
    rt_CID[0] -> GenericPostRouteProc -> CIDPostRouteProc -> [0]output;
    rt_CID[1] -> XIANextHop -> check_dest;
    rt_CID[2] -> consider_next_path;


    IPPostRouteProc :: XIAIPEncap(SRC $ip_addr);

    // Next destination is an IPv4 path
    c[4] -> rt_IP :: GenericRouting4Port;
    rt_IP[0] -> GenericPostRouteProc -> IPPostRouteProc -> [0]output;
    rt_IP[1] -> XIANextHop -> check_dest;
    rt_IP[2] -> consider_next_path;

    c[5] -> [2]output;
};

elementclass RouteEngine {
    $local_addr, $ip_addr |

    // $local_addr: the full address of the node (only used for debugging)

    // input[0]: a packet arrived at the node from outside (i.e. routing with caching)
    // input[1]: a packet to send from a node (i.e. routing without caching)
    // output[0]: forward (painted)
    // output[1]: arrived at destination node; go to RPC
    // output[2]: arrived at destination node; go to cache

    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    proc :: XIAPacketRoute($local_addr, $ip_addr);
    dstTypeClassifier :: XIAXIDTypeClassifier(dst CID, -);

    input[0] -> srcTypeClassifier;
    input[1] -> proc;

    srcTypeClassifier[0] -> cidFork :: Tee(2) -> [2]output;  // To cache (for content caching)
    cidFork[1] -> proc;                 // Main routing process

    srcTypeClassifier[1] -> proc;       // Main routing process

    proc[0] -> [0]output;               // Forward to other interface

    proc[1] -> dstTypeClassifier;
    dstTypeClassifier[1] -> [1]output;  // To RPC

    dstTypeClassifier[0] ->[2]output;  // To cache (for serving content request)

    proc[2] -> XIAPrint() -> Discard;  // No route drop (future TODO: return an error packet)
};

// 2-port router node
elementclass DualRouter {
    $local_addr, $local_ad, $local_hid, $host_hid, $local_ip, $local_mac |
    
    // $local_addr: the full address of the node
    // $local_ad:   the AD of the node and the local network
    // $local_hid:  the HID of the node (used for "bound" content source)
    // $local_ip:   the IPv4 address of the node

    // input[0], input[1]: a packet arrived at the node
    // output[0]: forward to interface 0 (for hosts in local ad)
    // output[1]: forward to interface 1 (for other ads)

    // output[0] is plugged into a host that only speaks XIA
    // output[1] is plugged into the broader internet and speaks XIA / IP

    n :: RouteEngine($local_addr, $local_ip);
    cache :: XIACache($local_addr, n/proc/rt_CID/rt);

    Script(write n/proc/rt_AD/rt.add - 5);      // default route for AD
    Script(write n/proc/rt_AD/rt.add $local_ad 4);    // self AD as destination
    Script(write n/proc/rt_HID/rt.add - 5);     // forwarding for local HID
    Script(write n/proc/rt_HID/rt.add $host_hid 0);     // forwarding for local HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self HID as destination
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path
    Script(write n/proc/rt_IP/rt.add - 1);
    //Script(write n/proc/rt_IP/rt.add $local_ip 4);


    arpt :: Tee(1);

    c1 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, 12/9999, -);

    input[1] -> c1;
    out1 :: Queue(200) -> [1]output;
    c1[0] -> ar1 :: ARPResponder($local_ip $local_mac) -> out1;
    arpq1 :: ARPQuerier($local_ip, $local_mac) -> out1;
    c1[1] -> arpt;
    arpt[0] -> [1]arpq1;
    c1[2] -> Paint(2) -> Strip(14) -> MarkIPHeader -> StripIPHeader -> MarkXIAHeader -> [0]n;
    c1[3] -> Paint(2) -> Strip(14) -> [0]n;
    c1[4] -> Print("eth1 non-IP") -> Discard;

    input[0] -> [0]n;
   // input[1] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    n[2] -> [0]cache[0] -> [1]n;
    Idle -> [1]cache[1] -> Discard;

    sw[0] -> Queue(200) -> [0]output;
    sw[1] -> arpq1; //out1;
};

// 1-port endhost node with sockets
elementclass EndHost {
    $local_addr, $local_hid,$fake,$CLICK_IP,$API_IP, $ether_addr, $enable_local_cache |

    // $local_addr: the full address of the node
    // $local_hid:  the HID of the node
    // $rpc_port:   the TCP port number to use for RPC

    // input: a packet arrived at the node
    // output: forward to interface 0
    
    n :: RouteEngine($local_addr, $CLICK_IP);
    xudp::XUDP($local_addr, $CLICK_IP,$API_IP,n/proc/rt_SID/rt);
    
    //Create kernel TAP interface which responds to ARP
    fake0::FromHost($fake,$API_IP/24,HEADROOM 256) 
    -> fromhost_cl :: Classifier(12/0806, 12/0800);
    fromhost_cl[0] -> ARPResponder(0.0.0.0/0 $ether_addr) -> ToHost($fake);

    //Classifier to sort between control/normal
    fromhost_cl[1]
    ->StripToNetworkHeader()
    ->sorter::IPClassifier(dst udp port 5001 or 5002 or 5003 or 5004 or 5005 or 5006,
                           dst udp port 10000 or 10001 or 10002);

    //Control in
    sorter[0]
    ->[0]xudp;

    //socket side data in
    sorter[1]
    ->[1]xudp;

    //socket side out
    xudp[1]->
    cIP::CheckIPHeader();
    cIP
    ->EtherEncap(0x0800, $ether_addr, 11:11:11:11:11:11)
    -> ToHost($fake)
    cIP[1]->Print(bad,MAXLENGTH 100, CONTENTS ASCII)->Discard();

    xudp[0]-> Discard;//Port 0 is unused for now.
    
    //To connect to forwarding instead of loopback
    //xudp[2]->Packet forwarding module
    //Packet forwarding module->[2]xudp0;

    cache :: XIACache($local_addr, n/proc/rt_CID/rt, $enable_local_cache);

    Script(write n/proc/rt_AD/rt.add - 0);      // default route for AD
    Script(write n/proc/rt_HID/rt.add - 0);     // default route for HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self HID as destination
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path
    Script(write n/proc/rt_IP/rt.add - 0); 	// default route for IPv4
    //Script(write n/proc/rt_IP/rt.add $local_ip 4);    // self IPv4 address as destination 

    input[0] -> n;
    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    n[1] -> srcTypeClassifier[1] -> [2]xudp[2] -> [0]n;
    srcTypeClassifier[0] -> Discard;    // do not send CID responses directly to RPC;
    
    n[2] -> [0]cache[0] -> [1]n;
    //For get and put cid
    xudp[3] -> [1]cache[1] -> [3]xudp;
    
    
    n -> Queue(200) -> [0]output;
    
};

elementclass IPRouter {
    // Generated by make-ip-conf.pl
    // eth0 18.26.4.92 00:00:C0:3B:71:EF
    // eth1 1.0.0.1 00:00:C0:CA:68:EF

    // Shared IP input path and routing table
    ip :: Strip(14)
    -> CheckIPHeader(INTERFACES 18.26.4.92/255.255.255.0 1.0.0.1/255.0.0.0)
    -> rt :: StaticIPLookup(
	18.26.4.92/32 0,
	18.26.4.255/32 0,
	18.26.4.0/32 0,
	1.0.0.1/32 0,
	1.255.255.255/32 0,
	1.0.0.0/32 0,
	18.26.4.0/255.255.255.0 1,
	1.0.0.0/255.0.0.0 2,
	255.255.255.255/32 0.0.0.0 0,
	0.0.0.0/32 0,
	0.0.0.0/0.0.0.0 18.26.4.1 1);

    // ARP responses are copied to each ARPQuerier and the host.
    arpt :: Tee(3);

    // Input and output paths for eth0
    c0 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
    //FromDevice(eth0) -> c0;
    input[0] -> c0;
    out0 :: Queue(200) -> [0]output; //todevice0 :: ToDevice(eth0);
    c0[0] -> Print("eth0 ARP Request") -> ar0 :: ARPResponder(18.26.4.92 1.0.0.0/8 00:00:C0:3B:71:EF) -> out0;
    arpq0 :: ARPQuerier(18.26.4.92, 00:00:C0:3B:71:EF) -> out0;
    c0[1] -> arpt;
    arpt[0] -> [1]arpq0;
    c0[2] -> IPPrint() -> Paint(1) -> ip;
    c0[3] -> Print("eth0 non-IP") -> Discard;

    // Input and output paths for eth1
    c1 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
    //FromDevice(eth1) -> c1;
    input[1] -> c1;
    out1 :: Queue(200) -> [1]output; //todevice1 :: ToDevice(eth1);
    c1[0] -> ar1 :: ARPResponder(1.0.0.1 00:00:C0:CA:68:EF) -> out1;
    arpq1 :: ARPQuerier(1.0.0.1, 00:00:C0:CA:68:EF) -> out1;
    c1[1] -> arpt;
    arpt[1] -> [1]arpq1;
    c1[2] -> Paint(2) -> ip;
    c1[3] -> Print("eth1 non-IP") -> Discard;

    // Local delivery
    //toh :: ToHost;
    //arpt[2] -> toh;
    arpt[2] -> Discard;    
    //rt[0] -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> toh;
    rt[0] -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> Discard;

    // Forwarding path for eth0
    rt[1] -> DropBroadcasts
    -> cp0 :: PaintTee(1)
    -> gio0 :: IPGWOptions(18.26.4.92)
    -> FixIPSrc(18.26.4.92)
    -> dt0 :: DecIPTTL
    -> fr0 :: IPFragmenter(1500)
    -> [0]arpq0;
    dt0[1] -> ICMPError(18.26.4.92, timeexceeded) -> rt;
    fr0[1] -> ICMPError(18.26.4.92, unreachable, needfrag) -> rt;
    gio0[1] -> ICMPError(18.26.4.92, parameterproblem) -> rt;
    cp0[1] -> ICMPError(18.26.4.92, redirect, host) -> rt;

    // Forwarding path for eth1
    rt[2] -> DropBroadcasts
    -> cp1 :: PaintTee(2)
    -> gio1 :: IPGWOptions(1.0.0.1)
    -> FixIPSrc(1.0.0.1)
    -> dt1 :: DecIPTTL
    -> fr1 :: IPFragmenter(1500)
    -> [0]arpq1;
    dt1[1] -> ICMPError(1.0.0.1, timeexceeded) -> rt;
    fr1[1] -> ICMPError(1.0.0.1, unreachable, needfrag) -> rt;
    gio1[1] -> ICMPError(1.0.0.1, parameterproblem) -> rt;
    cp1[1] -> ICMPError(1.0.0.1, redirect, host) -> rt;
}

// aliases for XIDs
XIAXIDInfo(
    HID0 HID:0000000000000000000000000000000000000000,
    HID1 HID:0000000000000000000000000000000000000001,
    AD0 AD:1000000000000000000000000000000000000000,
    AD1 AD:1000000000000000000000000000000000000001,
    RHID0 HID:0000000000000000000000000000000000000002,
    RHID1 HID:0000000000000000000000000000000000000003,
    CID0 CID:2000000000000000000000000000000000000001,
    RIP1 IP:4500000000010000FAFA00000000000001000002,
    //RIP1 IP:123456789ABCDEF13579ACE2468BDF124578ABDE,
);

    // eth0 18.26.4.92 00:00:C0:3B:71:EF
    // eth1 1.0.0.1 00:00:C0:CA:68:EF

// host & router instantiation
host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,1);
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,0);
router0 :: DualRouter(RE AD0 RHID0, AD0, RHID0, HID0, 18.26.4.30, 00:00:C0:3B:71:EE);
router1 :: DualRouter(RE AD1 RHID1, AD1, RHID1, HID1, 1.0.0.2, 00:00:C0:Ca:68:EE);
iprouter :: IPRouter;

// interconnection -- host - ad
host0[0] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] ->  Script(TYPE PACKET, print "router0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

host1[0] ->  Script(TYPE PACKET, print "host1 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] ->  Script(TYPE PACKET, print "router1 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) ->[0]host1;

// interconnection -- ad - ad
router0[1] -> Script(TYPE PACKET, print "router0 output1", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) ->[0]iprouter;
router1[1] -> Script(TYPE PACKET, print "router1 output1", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) ->[1]iprouter;

iprouter[0] -> Script(TYPE PACKET, print "iprouter output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [1]router0;
iprouter[1] -> Script(TYPE PACKET, print "iprouter output1", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;


// send test packets from host0 to host1

//gen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
//-> RatedUnqueue(5)
//-> XIAEncap(
//    DST RE RIP1 HID1,
//    SRC RE RIP0 HID0)
//-> AggregateCounter(COUNT_STOP 2)
//-> host0;

// send test packets from host1 to host0
//gen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
//-> RatedUnqueue(5)
//-> XIAEncap(
//    DST RE AD0 HID0,
//    SRC RE AD1 HID1)
//-> AggregateCounter(COUNT_STOP 1)
//-> host1;

//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed



/*ipgen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
    SRC RE AD0 HID0,
    DST RE AD1 HID1)
-> IPEncap(250, 18.26.4.30, 1.0.0.2) //-> IPPrint(CONTENTS HEX)
-> EtherEncap(0x0800, 00:00:C0:3B:71:EE, 00:00:C0:3B:71:EF)
-> AggregateCounter(COUNT_STOP 2)
-> [0]iprouter; */
/*
ipgen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
    SRC RE AD0 HID0,
    DST DAG			0 1 -	// -1
    	       AD1	       	2 -	// 0
    	       //IPXID(1.0.0.2) 	2 -	// 1
	       RIP1 		2 -	// 1
	       HID1		  	// 2
        , DYNAMIC false		) 
-> AggregateCounter(COUNT_STOP 2)
-> host0; */


//Script(write ipgen.active true);