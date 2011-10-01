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
    $local_addr |

    // $local_addr: the full address of the node (only used for debugging)

    // input: a packet to process
    // output[0]: forward (painted)
    // output[1]: arrived at destination node
    // output[2]: could not route at all (tried all paths)

    check_dest :: XIACheckDest();
    consider_first_path :: XIASelectPath(first);
    consider_next_path :: XIASelectPath(next);
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, -);

    //input -> Print("packet received by $local_addr") -> consider_first_path;
    input -> consider_first_path;

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

    c[4] -> [2]output;
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

    dstTypeClassifier[0] -> [2]output;  // To cache (for serving content request)

    proc[2] -> XIAPrint(no_route) -> Discard;  // No route drop (future TODO: return an error packet)
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

// 2-port router node
elementclass Router {
    $local_addr, $local_ad, $local_hid |

    // $local_addr: the full address of the node
    // $local_ad:   the AD of the node and the local network
    // $local_hid:  the HID of the node (used for "bound" content source)

    // input[0], input[1]: a packet arrived at the node
    // output[0]: forward to interface 0 (for hosts in local ad)
    // output[1]: forward to interface 1 (for other ads)

    n :: RouteEngine($local_addr);
    cache :: XIATransport($local_addr, n/proc/rt_CID/rt);

    Script(write n/proc/rt_AD/rt.add - 1);      // default route for AD
    Script(write n/proc/rt_AD/rt.add $local_ad 4);    // self AD as destination
    Script(write n/proc/rt_HID/rt.add - 0);     // forwarding for local HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self HID as destination
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path

    input[0] -> [0]n;
    input[1] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    n[2] -> [0]cache[0] -> [1]n;
    Idle -> [1]cache[1] -> Discard;

    sw[0] -> Queue(200) -> [0]output;
    sw[1] -> Queue(200) -> [1]output;
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
    cache :: XIATransport($local_addr, n/proc/rt_CID/rt);

    input[0] -> [0]n;
    input[1] -> [0]n;
    input[2] -> [0]n;
    input[3] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    n[2] -> [0]cache[0] -> [1]n;
    Idle -> [1]cache[1] -> Discard;

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
    //cache :: XIATransport($local_addr, n/proc/rt_CID/rt);
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


// aliases for XIDs
XIAXIDInfo(
    HID0 HID:0000000000000000000000000000000000000000,
    HID1 HID:0000000000000000000000000000000000000001,
    AD0 AD:1000000000000000000000000000000000000000,
    AD1 AD:1000000000000000000000000000000000000001,
    RHID0 HID:0000000000000000000000000000000000000002,
    RHID1 HID:0000000000000000000000000000000000000003,
    CID0 CID:2000000000000000000000000000000000000001,
);

// host & router instantiation
host0 :: Host(RE AD0 HID0, HID0, 2000, true);
//host1 :: Host(RE AD1 HID1, HID1, 2001, true);
router0 :: Router(RE AD0 RHID0, AD0, RHID0);
//router1 :: Router(RE AD1 RHID1, AD1, RHID1);

// interconnection -- host - ad
host0[0] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) -> Unqueue() -> [0]router0;
router0[0] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) -> Unqueue() -> [0]host0;

//host1[0] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
//router1[0] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) ->[0]host1;

// interconnection -- ad - ad
router0[1] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) 
-> MarkXIAHeader()
-> XIAPrint(out)
//-> Socket(UDP, 198.133.224.187, 8027);
//-> Socket(TCP, 128.2.208.168, 8027, CLIENT true);
//-> s1::Socket(TCP, 0.0.0.0, 8028, CLIENT false);
-> s1::Socket(UDP, 128.2.209.187, 5027, 198.133.224.187, 8027, SNAPLEN 50000, CLIENT true)
-> MarkXIAHeader() -> Print(in)->XIAPrint(in)-> Script(TYPE PACKET, print "host0 output0", print_realtime) ->[1]router0;
//s2::Socket(TCP, 0.0.0.0, 8027, CLIENT false) 
//s2::Socket(UDP, 0.0.0.0, 8028, SNAPLEN 50000)
//s1

// send test packets from host0 to host1
/*
gen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
    DST RE AD1 HID1,
    SRC RE AD0 HID0)
-> AggregateCounter(COUNT_STOP 1)
-> host0;
*/
// send test packets from host1 to host0
//gen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
//-> RatedUnqueue(5)
//-> XIAEncap(
//    DST RE AD0 HID0,
//    SRC RE AD1 HID1)
//-> AggregateCounter(COUNT_STOP 1)
//-> host1;

//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed

