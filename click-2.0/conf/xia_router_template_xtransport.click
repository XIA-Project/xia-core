elementclass GenericRouting4Port {
    $local_addr |
    // input: a packet to route
    // output[0]: forward to port 0~3 (painted)
    // output[1]: need to update "last" pointer
    // output[2]: no match

    rt :: XIAXIDRouteTable($local_addr);

    // previously painted according to the incoming inteface number	
    input -> sw :: PaintSwitch
    sw[0] -> [0]rt;
    sw[1] -> [1]rt;
    sw[2] -> [2]rt;
    sw[3] -> [3]rt;
    sw[4] -> [4]rt;  
    sw[5] -> [5]rt;
    sw[6] -> [6]rt;  // input port for XCMP redirect packet
 
    rt[0] -> Paint(0) -> [0]output;
    rt[1] -> Paint(1) -> [0]output;
    rt[2] -> Paint(2) -> [0]output;
    rt[3] -> Paint(3) -> [0]output;
    rt[4] -> [1]output; // destined for local host --> continue eval of dag
    rt[5] -> [2]output; // don't know where this packet should go --> eval fallbacks
    rt[6] -> [3]output;			// hack to use 6th port for DHCP
    
    rt[7] -> broadcast_pkt_Fork :: Tee(4) -> Paint(0) -> [0]output;     // outgoing broadcast packet
    broadcast_pkt_Fork[1] -> Paint(1) -> [0]output;   
    broadcast_pkt_Fork[2] -> Paint(2) -> [0]output; 
    broadcast_pkt_Fork[3] -> Paint(3) -> [0]output;   

    rt[8] -> x::XCMP($local_addr) -> MarkXIAHeader() -> Paint(4) -> [4]output; // need to reprocess redirect
    x[1] -> Discard;
    rt[9] -> Discard; // packet dropping (e.g., incoming XCMP redirect packets will be dropped after being procecced) 
};

elementclass GenericPostRouteProc {
    decH :: XIADecHLIM;
    input -> decH[0] -> output;
    decH[1] -> [1]output;
};

elementclass XIAPacketRoute {
    $local_addr |

    // $local_addr: the full address of the node (only used for debugging)

    // input: a packet to process
    // output[0]: forward (painted)
    // output[1]: arrived at destination node
    // output[2]: could not route at all (tried all paths)

    check_dest :: XIACheckDest();
    consider_first_path :: XIASelectPath(first);
    consider_next_path :: XIASelectPath(next);
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, next IP, -);

    //input -> Print("packet received by $local_addr") -> consider_first_path;
    input -> consider_first_path;

    check_dest[0] -> [1]output;             // arrived at the final destination
    check_dest[1] -> consider_first_path;   // reiterate paths with new last pointer

    consider_first_path[0] -> c;
    consider_first_path[1] -> [2]output;
    consider_next_path[0] -> c;
    consider_next_path[1] -> [2]output;

    GPRP :: GenericPostRouteProc;
    x :: XCMP($local_addr);

    //  Next destination is AD
    c[0] -> rt_AD :: GenericRouting4Port($local_addr);
    rt_AD[0] -> GPRP;
    rt_AD[1] -> XIANextHop -> check_dest;
    rt_AD[2] -> consider_next_path;
	rt_AD[4] -> consider_first_path; // possible xcmp redirect message

    //  Next destination is HID
    c[1] -> rt_HID :: GenericRouting4Port($local_addr);
    rt_HID[0] -> GPRP;
    rt_HID[1] -> XIANextHop -> check_dest;
    rt_HID[2] -> consider_next_path;
	rt_HID[4] -> consider_first_path; // possible xcmp redirect message

    //  Next destination is SID
    c[2] -> rt_SID :: GenericRouting4Port($local_addr);
    rt_SID[0] -> GPRP;
    rt_SID[1] -> XIANextHop -> check_dest;
    rt_SID[2] -> consider_next_path;
    rt_SID[3] -> [3]output;				// hack to use DHCP functionality
	rt_SID[4] -> consider_first_path; // possible xcmp redirect message


    // change this if you want to do CID post route processing for any reason
    CIDPostRouteProc :: Null;

    //  Next destination is CID
    c[3] -> rt_CID :: GenericRouting4Port($local_addr);
    GPRP_CID :: GenericPostRouteProc;
    rt_CID[0] -> GPRP_CID -> CIDPostRouteProc -> [0]output;
    rt_CID[1] -> XIANextHop -> check_dest;
    rt_CID[2] -> consider_next_path;
	rt_CID[4] -> consider_first_path; // possible xcmp redirect message
    GPRP_CID[1] -> [0]x;


    // Next destination is an IPv4 path
    c[4] -> rt_IP :: GenericRouting4Port($local_addr);
    rt_IP[0] -> GPRP;
    rt_IP[1] -> XIANextHop -> check_dest;
    rt_IP[2] -> consider_next_path;
	rt_IP[4] -> consider_first_path; // possible xcmp redirect message

    GPRP[0] -> [0]output;
    GPRP[1] -> Print("TIME EXCEEDED") -> x[0] -> consider_first_path;
    x[1] -> Discard;

    c[5] -> [2]output;

    // port 3 used only for SID, hack to use DHCP functionality
    rt_AD[3] -> Discard();
    rt_HID[3] -> Discard();
    rt_CID[3] -> Discard();
    rt_IP[3] -> Discard();
};

elementclass RouteEngine {
    $local_addr |

    // $local_addr: the full address of the node (only used for debugging)

    // input[0]: a packet arrived at the node from outside (i.e. routing with caching)
    // input[1]: a packet to send from a node (i.e. routing without caching)
    // output[0]: forward (painted)
    // output[1]: arrived at destination node; go to RPC
    // output[2]: arrived at destination node; go to cache

    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    proc :: XIAPacketRoute($local_addr);
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

	x :: XCMP($local_addr);

    proc[2] -> Paint(65) -> x -> MarkXIAHeader() -> Paint(5) -> proc; 
	//XIAPrint("Drop") -> Discard;  // No route drop (future TODO: return an error packet)
	x[1] -> Discard;
  
    // hack to use DHCP functionality
    proc[3] -> [3]output;
};

// 1-port host node
elementclass Host {
    $local_addr, $local_hid, $rpc_port, $enable_local_cache |

    // $local_addr: the full address of the node
    // $local_hid:  the HID of the node
    // $rpc_port:   the TCP port number to use for RPC

    // input: a packet arrived at the node
    // output: forward to interface 0

    n :: RouteEngine($local_addr);
    sock :: Socket(TCP, 0.0.0.0, $rpc_port, CLIENT false);
    rpc :: XIARPC($local_addr);
    cache :: XIATransport($local_addr, n/proc/rt_CID/rt, $enable_local_cache);

    Script(write n/proc/rt_AD/rt.add - 0);      // default route for AD
    Script(write n/proc/rt_HID/rt.add - 0);     // default route for HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self HID as destination
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path

    input[0] -> n;

    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);

    sock -> [0]rpc[0] -> sock;
    n[1] -> srcTypeClassifier[1] -> [1]rpc[1] -> [0]n;
    srcTypeClassifier[0] -> Discard;    // do not send CID responses directly to RPC;
                                        // they must be served by cache using the following connection only
    n[2] -> [0]cache[0] -> [1]n;
    rpc[2] -> [1]cache[1] -> [2]rpc;

    n -> Queue(200) -> [0]output;
};


// 2-port router node (DEPRECATED: please use XRouter4Port or XRouter2Port)
elementclass Router {
    $local_addr, $local_ad, $local_hid |

    // $local_addr: the full address of the node
    // $local_ad:   the AD of the node and the local network
    // $local_hid:  the HID of the node (used for "bound" content source)

    // input[0], input[1]: a packet arrived at the node
    // output[0]: forward to interface 0 (for hosts in local ad)
    // output[1]: forward to interface 1 (for other ads)

    n :: RouteEngine($local_addr);
    cache :: XIACache($local_addr, n/proc/rt_CID/rt, PACKET_SIZE 1400); // specify XIA packet size (including the XIA + content header) 
    
    
    Script(write n/proc/rt_AD/rt.add - 1);      // default route for AD
    Script(write n/proc/rt_AD/rt.add $local_ad 4);    // self AD as destination
    Script(write n/proc/rt_HID/rt.add - 0); 	// default route for HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self RHID as destination
    Script(write n/proc/rt_HID/rt.add BHID 7);  // outgoing broadcast packet
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path
    //Script(write n/proc/rt_IP/rt.add - 0); 	// default route for IPv4        

    c :: Classifier(01/3D, -); // XCMP
    x :: XCMP($local_addr);

    input[0] -> [0]n;
    input[1] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> c[1] -> Discard;
    c[0] -> x; //IPPrint("going into XCMP Module", CONTENTS HEX) -> x;
    x[0] -> [0]n; // new (response) XCMP packets destined for some other machine
    x[1] -> Discard; // XCMP packet actually destined for this router??
    n[2] -> [0]cache[0] -> [1]n;
    Idle -> [1]cache[1] -> Discard;

    sw[0] -> Queue(200) -> [0]output;
    sw[1] -> Queue(200) -> [1]output;

    n[3] -> Discard();
};


// 2-port XIA and IP speaking router node
elementclass DualRouter {
    $local_addr, $local_ad, $local_hid, $host_hid, $local_ip, $local_mac |
    
    // $local_addr: the full address of the node
    // $local_ad:   the AD of the node and the local network
    // $local_hid:  the HID of the node (used for "bound" content source)
    // $local_ip:   the IPv4 address of the node
    // $local_mac:  the MAC address of the node

    // input[0], input[1]: a packet arrived at the node
    // output[0] is plugged into a host that only speaks XIA
    // output[1] is plugged into the broader internet and speaks XIA / IP

    n :: RouteEngine($local_addr);
    cache :: XIACache($local_addr, n/proc/rt_CID/rt);

    Script(write n/proc/rt_AD/rt.add - 5);      // default route for AD
    Script(write n/proc/rt_AD/rt.add $local_ad 4);    // self AD as destination
    Script(write n/proc/rt_HID/rt.add - 5);     // forwarding for local HID
    Script(write n/proc/rt_HID/rt.add $host_hid 0);     // forwarding for local HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self HID as destination
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path
    Script(write n/proc/rt_IP/rt.add IP:$local_ip 4); // self IP as destination
    Script(write n/proc/rt_IP/rt.add - 1);	// default route for IP traffic

    arpt :: Tee(2);

    c0 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800 23/11 36/03E9, 12/9999, -);

    input[0] -> c0;
    out0 :: Queue(200) -> [0]output;
    c0[0] -> Discard; //ar0 :: ARPResponder($local_ip/32 $local_mac) -> out0;
    arpq0 :: ARPQuerier($local_ip, $local_mac) -> out0;
    c0[1] -> arpt;
    arpt[0] -> [1]arpq0;
    c0[2] -> Paint(1) -> Strip(14) -> MarkIPHeader -> StripIPHeader -> Strip(8) -> MarkXIAHeader -> [0]n;
    c0[3] -> Paint(1) -> Strip(14) -> [0]n;
    //c0[4] -> Print("eth0 non-IP/XIA") -> Discard;
    c0[4] -> [0]n; // Print("eth0 non-IP") -> Discard;

    c1 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800 23/11 36/03E9, 12/9999, -);

    input[1] -> c1;
    out1 :: Queue(200) -> [1]output;
    c1[0] -> Discard; //ar1 :: ARPResponder($local_ip/32 $local_mac) -> out1;
    arpq1 :: ARPQuerier($local_ip, $local_mac) -> out1;
    c1[1] -> arpt;
    arpt[1] -> [1]arpq1;
    c1[2] -> Paint(2) -> Strip(14) -> MarkIPHeader -> StripIPHeader -> Strip(8) -> MarkXIAHeader -> [0]n;
    c1[3] -> Paint(2) -> Strip(14) -> [0]n;
    //c1[4] -> IPPrint(CONTENTS HEX) -> Print("non-IP/XIA") -> Discard;
    c1[4] -> Discard;

    dstTypeC :: XIAXIDTypeClassifier(next IP, -);
    swIP :: PaintSwitch;
    swXIA :: PaintSwitch;

    n[0] -> dstTypeC;
    dstTypeC[0] -> XIAIPEncap(SRC $local_ip) -> swIP;
    dstTypeC[1] -> swXIA;    

    n[1] -> Discard;
    n[2] -> [0]cache[0] -> [1]n;
    Idle -> [1]cache[1] -> Discard;

    dip :: DirectIPLookup(0.0.0.0/0 $local_ip:gw 0);
    swIP[0] -> arpq0;
    swIP[1] -> dip -> arpq1;

    swXIA[0] -> out0;
    swXIA[1] -> out1;
};

// 4-port router node; AD & HID tables need to be set manually using Script()
elementclass Router4Port {
    $local_addr |

    // $local_addr: the full address of the node

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3

    n :: RouteEngine($local_addr);
    cache :: XIACache($local_addr, n/proc/rt_CID/rt, PACKET_SIZE 1400);

    input[0] -> [0]n;
    input[1] -> [0]n;
    input[2] -> [0]n;
    input[3] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    n[2] -> [0]cache[0] -> [1]n;
    Idle -> [1]cache[1] -> Discard;
    
    n[3] -> Discard();

    sw[0] -> Queue(200) -> [0]output;
    sw[1] -> Queue(200) -> [1]output;
    sw[2] -> Queue(200) -> [2]output;
    sw[3] -> Queue(200) -> [3]output;
};

// 4-port router node with "dummy cache" (for microbench)
elementclass Router4PortDummyCache {
    $local_addr |

    // $local_addr: the full address of the node

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3

    n :: RouteEngine($local_addr);
    //cache :: XIACache($local_addr, n/proc/rt_CID/rt);
    cache :: Queue(200);

    input[0] -> [0]n;
    input[1] -> [0]n;
    input[2] -> [0]n;
    input[3] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    //n[2] -> [0]cache[0] -> [1]n;
    //Idle -> [1]cache[1] -> Discard;
    Idle -> [1]n;
    n[2] -> cache -> Unqueue -> Discard;

    sw[0] -> Queue(200) -> [0]output;
    sw[1] -> Queue(200) -> [1]output;
    sw[2] -> Queue(200) -> [2]output;
    sw[3] -> Queue(200) -> [3]output;
};

// IP router node (caution: simplified version for forwarding experiments)
elementclass IPRouter4Port {
    $local_addr |

    // $local_addr: the full address of the node

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3

    //rt :: RangeIPLookup;    // fastest lookup for large table
    //rt :: DirectIPLookup;   // 5% faster than RangeIPLookup for very small table
    rt :: RadixIPLookup;    // fastest to setup, most generous (no limits in numbers)
    dt :: DecIPTTL;
    fr :: IPFragmenter(1500);

    input[0] -> rt;
    input[1] -> rt;
    input[2] -> rt;
    input[3] -> rt;

    rt[0] -> Paint(0) -> dt;
    rt[1] -> Paint(1) -> dt;
    rt[2] -> Paint(2) -> dt;
    rt[3] -> Paint(3) -> dt;

    dt -> fr -> sw :: PaintSwitch;

    dt[1] -> Print("time exceeded") -> Discard; // ICMPError($local_addr, timeexceeded) -> sw;
    fr[1] -> Print("need fragmentation") -> Discard; // ICMPError($local_addr, unreachable, needfrag) -> sw;

    sw[0] -> Queue(200) -> [0]output;
    sw[1] -> Queue(200) -> [1]output;
    sw[2] -> Queue(200) -> [2]output;
    sw[3] -> Queue(200) -> [3]output;
};

// IPv6 router node (caution: simplified version for forwarding experiments)
elementclass IP6Router4Port {
    $local_addr |

    // $local_addr: the full address of the node

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3

    rt :: LookupIP6Route;
    dt :: DecIP6HLIM;

    input[0] -> rt;
    input[1] -> rt;
    input[2] -> rt;
    input[3] -> rt;

    rt[0] -> Paint(0) -> dt;
    rt[1] -> Paint(1) -> dt;
    rt[2] -> Paint(2) -> dt;
    rt[3] -> Paint(3) -> dt;

    dt -> sw :: PaintSwitch;

    dt[1] -> Print("time exceeded") -> Discard; // ICMPError($local_addr, timeexceeded) -> sw;

    sw[0] -> Queue(200) -> [0]output;
    sw[1] -> Queue(200) -> [1]output;
    sw[2] -> Queue(200) -> [2]output;
    sw[3] -> Queue(200) -> [3]output;
};



// 2-port router node : DEPRECATED
elementclass XRouter2Port {
    $local_addr, $local_ad, $local_hid, $fake, $CLICK_IP, $API_IP, $ether_addr, $mac0, $mac1 |

    // $local_addr: the full address of the node
    // $local_ad:   the AD of the node and the local network
    // $local_hid:  the HID of the node (used for "bound" content source)

    // input[0], input[1]: a packet arrived at the node
    // output[0]: forward to interface 0 (for hosts in local ad)
    // output[1]: forward to interface 1 (for other ads)

    n :: RouteEngine($local_addr);
    xtransport::XTRANSPORT($local_addr, $CLICK_IP, $API_IP, n/proc/rt_SID/rt);    
    cache :: XIACache($local_addr, n/proc/rt_CID/rt, PACKET_SIZE 1400); // specify XIA packet size (including the XIA + content header)


    //Create kernel TAP interface which responds to ARP
    fake0::FromHost($fake, $API_IP/24, CLICK_XTRANSPORT_ADDR $CLICK_IP ,HEADROOM 256, MTU 65521) 
    //-> Print()
    -> fromhost_cl :: Classifier(12/0806, 12/0800);
    fromhost_cl[0] -> ARPResponder(0.0.0.0/0 $ether_addr) -> ToHost($fake);

    //Classifier to sort between control/normal
    fromhost_cl[1]
    ->StripToNetworkHeader()
    ->sorter::IPClassifier(dst udp port 5001 or 5002 or 5003 or 5004 or 5005 or 5006,
                           dst udp port 10000 or 10001 or 10002);

    //Control in
    sorter[0]
    ->[0]xtransport;

    //socket side data in
    sorter[1]
    ->[1]xtransport;

    //socket side out
    xtransport[1]->
    cIP::CheckIPHeader();
    cIP
    ->EtherEncap(0x0800, $ether_addr, 11:11:11:11:11:11)
    -> ToHost($fake)
    cIP[1]->Print(bad,MAXLENGTH 100, CONTENTS ASCII)->Discard();

    xtransport[0]-> Discard;//Port 0 is unused for now.
    
    //To connect to forwarding instead of loopback
    //xtransport[2]->Packet forwarding module
    //Packet forwarding module->[2]xtransport0;


    Script(write n/proc/rt_AD/rt.add - 1);      // default route for AD
    Script(write n/proc/rt_AD/rt.add $local_ad 4);    // self AD as destination
    Script(write n/proc/rt_HID/rt.add - 0); 	// default route for HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self RHID as destination
    Script(write n/proc/rt_HID/rt.add BHID 7);  // outgoing broadcast packet
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path
    //Script(write n/proc/rt_IP/rt.add - 0); 	// default route for IPv4      

    // quick fix
    n[3] -> Discard();
    Idle() -> [4]xtransport;
    

    // set up XCMP elements
    c :: Classifier(01/3D, -); // XCMP
    x :: XCMP($local_addr);
    
    
    // setup XARP module0
    c0 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
    xarpq0 :: XARPQuerier($local_hid, $mac0);
    xarpr0 :: XARPResponder($local_hid $mac0);        
    
    // On receiving a packet from Interface0	
    input[0] -> c0; 
    
    // Receiving an XIA packet
    c0[2] -> Strip(14) -> MarkXIAHeader() -> Paint(0) -> [0]n; 
     
    out0 :: Queue(200) -> [0]output;

    // On receiving XARP response
    c0[1] -> [1]xarpq0 -> out0;
  
    // On receiving XARP query
    c0[0] -> xarpr0 -> out0;
    
    // XAPR timeout to XCMP
    xarpq0[1] -> x;
    
    
        

    // setup XARP module1
    c1 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
    xarpq1 :: XARPQuerier($local_hid, $mac1);
    xarpr1 :: XARPResponder($local_hid $mac1);        
    
    // On receiving a packet from Interface1	
    input[1] -> c1; 
    
    // Receiving an XIA packet
    c1[2] -> Strip(14) -> MarkXIAHeader() -> Paint(1) -> [0]n; 

    out1 :: Queue(200) -> [1]output;
     
    // On receiving XARP response
    c1[1] -> [1]xarpq1 -> out1;
  
    // On receiving XARP query
    c1[0] -> xarpr1 -> out1;
    
    // XAPR timeout to XCMP
    xarpq1[1] -> x; 
    
     

    n[0] -> sw :: PaintSwitch
    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    n[1] -> c[1] -> srcTypeClassifier[1] -> [2]xtransport[2] -> Paint(4) -> [0]n;
    srcTypeClassifier[0] -> Discard;    // do not send CID responses directly to RPC;
    c[0] -> x; //IPPrint("going into XCMP Module", CONTENTS HEX) -> x;
    x[0] -> [0]n; // new (response) XCMP packets destined for some other machine
    x[1] -> Discard; // XCMP packet actually destined for this router??
    n[2] -> [0]cache[0] -> Paint(4) -> [1]n;
    //For get and put cid
    xtransport[3] -> [1]cache[1] -> [3]xtransport;


    // Sending an XIP packet (via XARP if necessary)
    sw[0] -> [0]xarpq0;
    sw[1] -> [0]xarpq1;    
    sw[2] -> Discard;
    sw[3] -> Discard;

};



// 4-port router node with XRoute process running
elementclass XRouter4Port {
    $local_addr, $local_ad, $local_hid, $fake, $CLICK_IP, $API_IP, $ether_addr, $mac0, $mac1, $mac2, $mac3 |


    // $local_addr: the full address of the node

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3
    
    
    n :: RouteEngine($local_addr);
    xtransport::XTRANSPORT($local_addr, $CLICK_IP, $API_IP, n/proc/rt_SID/rt);        
    
    //Create kernel TAP interface which responds to ARP
    fake0::FromHost($fake, $API_IP/24, CLICK_XTRANSPORT_ADDR $CLICK_IP ,HEADROOM 256, MTU 65521) 
    //-> Print()
    -> fromhost_cl :: Classifier(12/0806, 12/0800);
    fromhost_cl[0] -> ARPResponder(0.0.0.0/0 $ether_addr) -> ToHost($fake);

    //Classifier to sort between control/normal
    fromhost_cl[1]
    ->StripToNetworkHeader()
    ->sorter::IPClassifier(dst udp port 5001 or 5002 or 5003 or 5004 or 5005 or 5006,
                           dst udp port 10000 or 10001 or 10002);

    //Control in
    sorter[0]
    ->[0]xtransport;

    //socket side data in
    sorter[1]
    ->[1]xtransport;

    //socket side out
    xtransport[1]->
    cIP::CheckIPHeader();
    cIP
    ->EtherEncap(0x0800, $ether_addr, 11:11:11:11:11:11)
    -> ToHost($fake)
    cIP[1]->Print(bad,MAXLENGTH 100, CONTENTS ASCII)->Discard();

    xtransport[0]-> Discard;//Port 0 is unused for now.
    
    //To connect to forwarding instead of loopback
    //xtransport[2]->Packet forwarding module
    //Packet forwarding module->[2]xtransport0;

    cache :: XIACache($local_addr, n/proc/rt_CID/rt, PACKET_SIZE 1400);

    //Script(write n/proc/rt_AD/rt.add - 0);      // default route for AD
    //Script(write n/proc/rt_HID/rt.add - 0);     // default route for HID
	//Script(write n/proc/rt_HID/rt.add HID1 0); // useful for testing xcmp redirect
    Script(write n/proc/rt_AD/rt.add $local_ad 4);    // self AD as destination
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self RHID as destination
    Script(write n/proc/rt_HID/rt.add BHID 7);  // outgoing broadcast packet
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path
    //Script(write n/proc/rt_IP/rt.add - 0); 	// default route for IPv4    

    // quick fix
    n[3] -> Discard();
    Idle() -> [4]xtransport;
    

    // set up XCMP elements
    c :: Classifier(01/3D, -); // XCMP
    x :: XCMP($local_addr);

    
    
    // setup XARP module0
    c0 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
    xarpq0 :: XARPQuerier($local_hid, $mac0);
    xarpr0 :: XARPResponder($local_hid $mac0);        
    
    // On receiving a packet from Interface0	
    input[0] -> c0; 
    
    // Receiving an XIA packet
    c0[2] -> Strip(14) -> MarkXIAHeader() -> Paint(0) -> [0]n; 

    out0 :: Queue(200) -> [0]output;
     
    // On receiving XARP response
    c0[1] -> [1]xarpq0 -> out0;
  
    // On receiving XARP query
    c0[0] -> xarpr0 -> out0;
    
    // XAPR timeout to XCMP
    xarpq0[1] -> x; 
    
    
    
    
    // setup XARP module1
    c1 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
    xarpq1 :: XARPQuerier($local_hid, $mac1);
    xarpr1 :: XARPResponder($local_hid $mac1);        
    
    // On receiving a packet from Interface1	
    input[1] -> c1; 
    
    // Receiving an XIA packet
    c1[2] -> Strip(14) -> MarkXIAHeader() -> Paint(1) -> [0]n; 
    
    out1 :: Queue(200) -> [1]output;
     
    // On receiving XARP response
    c1[1] -> [1]xarpq1 -> out1;
  
    // On receiving XARP query
    c1[0] -> xarpr1 -> out1;
    
    // XAPR timeout to XCMP
    xarpq1[1] -> x; 
    
    

        
    // setup XARP module2
    c2 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
    xarpq2 :: XARPQuerier($local_hid, $mac2);
    xarpr2 :: XARPResponder($local_hid $mac2);        
    
    // On receiving a packet from Interface2	
    input[2] -> c2; 
    
    // Receiving an XIA packet
    c2[2] -> Strip(14) -> MarkXIAHeader() -> Paint(2) -> [0]n; 

    out2 :: Queue(200) -> [2]output;
     
    // On receiving XARP response
    c2[1] -> [1]xarpq2 -> out2;
  
    // On receiving XARP query
    c2[0] -> xarpr2 -> out2;
    
    // XAPR timeout to XCMP
    xarpq2[1] -> x; 
    
        
        
    
    // setup XARP module3
    c3 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
    xarpq3 :: XARPQuerier($local_hid, $mac3);
    xarpr3 :: XARPResponder($local_hid $mac3);        

    // On receiving a packet from Interface3	
    input[3] -> c3; 
    
    // Receiving an XIA packet
    c3[2] -> Strip(14) -> MarkXIAHeader() -> Paint(3) -> [0]n; 

    out3 :: Queue(200) -> [3]output;
     
    // On receiving XARP response
    c3[1] -> [1]xarpq3 -> out3;
  
    // On receiving XARP query
    c3[0] -> xarpr3 -> out3;

    // XAPR timeout to XCMP
    xarpq3[1] -> x; 
    
    
    
       

    n[0] -> sw :: PaintSwitch
    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    n[1] -> c[1] -> srcTypeClassifier[1] -> [2]xtransport[2] -> Paint(4) -> [0]n;
    srcTypeClassifier[0] -> Discard;    // do not send CID responses directly to RPC;
    c[0] -> x; //IPPrint("going into XCMP Module", CONTENTS HEX) -> x;
    x[0] -> [0]n; // new (response) XCMP packets destined for some other machine
    x[1] -> Discard; // XCMP packet actually destined for this router??
    n[2] -> [0]cache[0] -> Paint(4) -> [1]n;
    //For get and put cid
    xtransport[3] -> [1]cache[1] -> [3]xtransport;
    
    
    // Sending an XIP packet (via XARP if necessary)
    sw[0] -> [0]xarpq0;
    sw[1] -> [0]xarpq1;
    sw[2] -> [0]xarpq2;
    sw[3] -> [0]xarpq3;
    
        
};





// 1-port endhost node with sockets
elementclass EndHost {
    $local_addr, $local_hid, $fake, $CLICK_IP, $API_IP, $ether_addr, $enable_local_cache, $mac |


    // $local_addr: the full address of the node
    // $local_hid:  the HID of the node
    // $rpc_port:   the TCP port number to use for RPC

    // input: a packet arrived at the node
    // output: forward to interface 0
    
    n :: RouteEngine($local_addr);
    xtransport::XTRANSPORT($local_addr, $CLICK_IP,$API_IP,n/proc/rt_SID/rt);   
    
    //Create kernel TAP interface which responds to ARP
    fake0::FromHost($fake, $API_IP/24, CLICK_XTRANSPORT_ADDR $CLICK_IP ,HEADROOM 256, MTU 65521) 
    //-> Print()
    -> fromhost_cl :: Classifier(12/0806, 12/0800);
    fromhost_cl[0] -> ARPResponder(0.0.0.0/0 $ether_addr) -> ToHost($fake);

    //Classifier to sort between control/normal
    fromhost_cl[1]
    ->StripToNetworkHeader()
    ->sorter::IPClassifier(dst udp port 5001 or 5002 or 5003 or 5004 or 5005 or 5006,
                           dst udp port 10000 or 10001 or 10002);

    //Control in
    sorter[0]
    ->[0]xtransport;

    //socket side data in
    sorter[1]
    ->[1]xtransport;

    //socket side out
    xtransport[1]->
    cIP::CheckIPHeader();
    cIP
    ->EtherEncap(0x0800, $ether_addr, 11:11:11:11:11:11)
    -> ToHost($fake)
    cIP[1]->Print(bad,MAXLENGTH 100, CONTENTS ASCII)->Discard();

    xtransport[0]-> Discard;//Port 0 is unused for now.
    
    //To connect to forwarding instead of loopback
    //xtransport[2]->Packet forwarding module
    //Packet forwarding module->[2]xtransport0;

    cache :: XIACache($local_addr, n/proc/rt_CID/rt, $enable_local_cache, PACKET_SIZE 1400);

    Script(write n/proc/rt_AD/rt.add - 0);      // default route for AD
    Script(write n/proc/rt_HID/rt.add - 0);     // default route for HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self HID as destination
    Script(write n/proc/rt_HID/rt.add BHID 7);  // outgoing broadcast packet
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path
    Script(write n/proc/rt_IP/rt.add - 0); 	// default route for IPv4    

    // setup XCMP
    c :: Classifier(01/3D, -); // XCMP
    x :: XCMP($local_addr);

    // quick fix
    n[3] -> Discard();
    Idle() -> [4]xtransport;

    
    // setup XARP module
    c0 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
    xarpq0 :: XARPQuerier($local_hid, $mac);
    xarpr0 :: XARPResponder($local_hid $mac);        
    
    // On receiving a packet from Interface0	
    input[0] -> c0; 
    
    // Receiving an XIA packet
    c0[2] -> Strip(14) -> MarkXIAHeader() -> Paint(0) -> [0]n; 

    out0 :: Queue(200) -> [0]output;
     
    // On receiving XARP response
    c0[1] -> [1]xarpq0 -> out0;
  
    // On receiving XARP query
    c0[0] -> xarpr0 -> out0;

    // XAPR timeout to XCMP
    xarpq0[1] -> x; 

    
    
    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    n[1] -> c[1] -> srcTypeClassifier[1] -> [2]xtransport[2] -> Paint(4) ->  [0]n;
    c[0] -> x; //IPPrint("going into XCMP Module", CONTENTS HEX) -> x;
    x[0] -> Paint(4) -> [0]n; // new (response) XCMP packets destined for some other machine
    x[1] -> rsw :: PaintSwitch -> //Print("XCMP going to XTransport") -> 
		 [2]xtransport; // XCMP packets destined for this machine
    rsw[1] -> Paint(6) -> [0]n; // XCMP redirect packet, so a route update will be done.

    srcTypeClassifier[0] -> Discard;    // do not send CID responses directly to RPC;
    
    n[2] -> [0]cache[0] -> [1]n;
    //For get and put cid
    xtransport[3] -> [1]cache[1] -> [3]xtransport;
    
    n[0] -> sw :: PaintSwitch
    
    // Sending an XIP packet (via XARP if necessary)
    sw[0] -> [0]xarpq0;
    sw[1] -> Discard();
    sw[2] -> Discard();
    sw[3] -> Discard();
    
};


