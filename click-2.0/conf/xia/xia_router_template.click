elementclass GenericRouting4Port {
    // input: a packet to route
    // output[0]: forward to port 0~3 (painted)
    // output[1]: need to update "last" pointer
    // output[2]: no match

    x :: XCMP;

    input ->  rt :: XIAXIDRouteTable;
    rt[0] ->  t0 :: Tee -> Paint(0) -> [0]output;
    rt[1] ->  t1 :: Tee -> Paint(1) -> [0]output;
    rt[2] ->  t2 :: Tee -> Paint(2) -> [0]output;
    rt[3] ->  t3 :: Tee -> Paint(3) -> [0]output;
    rt[4] ->  [1]output;
    rt[5] ->  [2]output;

    t0[1] -> CheckPaint(0) -> x;
    t1[1] -> CheckPaint(1) -> x;
    t2[1] -> CheckPaint(2) -> x;
    t3[1] -> CheckPaint(3) -> x;

    x -> [0]output;
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
    input 
//	-> XIAPrint("consider first_path")
	-> consider_first_path;

    check_dest[0] -> [1]output;             // arrived at the final destination
    check_dest[1] -> consider_first_path;   // reiterate paths with new last pointer

    consider_first_path[0] -> c;
    consider_first_path[1] -> [2]output;
    consider_next_path[0] 
//	-> XIAPrint("consider next")
	-> c;
    consider_next_path[1] -> [2]output;

    //  Next destination is AD
    c[0] 
//	-> XIAPrint("AD routing")
	-> rt_AD :: GenericRouting4Port;
    rt_AD[0] 
//	-> XIAPrint("AD success")
	-> GenericPostRouteProc -> [0]output;
    rt_AD[1] 
//	-> XIAPrint("AD nexthop")
	-> XIANextHop -> check_dest;
    rt_AD[2] 
//	-> XIAPrint("AD fail nextpath")
	-> consider_next_path;

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
    c[3] 
//	-> XIAPrint("CID")
	-> rt_CID :: GenericRouting4Port;
    rt_CID[0] 
//	-> XIAPrint("CID success")
	-> GenericPostRouteProc -> CIDPostRouteProc -> [0]output;
    rt_CID[1] 
//	-> XIAPrint("CID thisnode")
	-> XIANextHop -> check_dest;
    rt_CID[2] 
//	-> XIAPrint("CID failed")
	-> consider_next_path;

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

    srcTypeClassifier[0] 
//	-> XIAPrint(Content) 
	-> cidFork :: Tee(2) -> [2]output;  // To cache (for content caching)

    cidFork[1] -> proc;                 // Main routing process

    srcTypeClassifier[1] -> proc;       // Main routing process

    proc[0] -> [0]output;               // Forward to other interface

    proc[1] -> dstTypeClassifier;
    dstTypeClassifier[1] -> [1]output;  // To RPC

    dstTypeClassifier[0] -> [2]output;  // To cache (for serving content request)

    proc[2] -> XIAPrint(NoRoute) -> Discard;  // No route drop (future TODO: return an error packet)
};

// 2-port dummy router node
elementclass RouterDummyCache {
    $local_addr, $local_ad, $local_hid |

    // $local_addr: the full address of the node
    // $local_ad:   the AD of the node and the local network
    // $local_hid:  the HID of the node (used for "bound" content source)

    // input[0], input[1]: a packet arrived at the node
    // output[0]: forward to interface 0 (for hosts in local ad)
    // output[1]: forward to interface 1 (for other ads)

    n :: RouteEngine($local_addr);
    //cache :: Queue(200);

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
    //n[2] -> [0]cache[0] -> [1]n;
    //Idle -> [1]cache[1] -> Discard;
    Idle -> [1]n;
    //n[2] -> cache -> Unqueue -> Discard;
    n[2]-> Discard;

    sw[0] -> IsoCPUQueue(200) -> [0]output;
    sw[1] -> IsoCPUQueue(200) -> [1]output;
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

    sw[0] -> IsoCPUQueue(200) -> [0]output;
    sw[1] -> IsoCPUQueue(200) -> [1]output;
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

    sw[0] -> IsoCPUQueue(200) -> [0]output;
    sw[1] -> IsoCPUQueue(200) -> [1]output;
    sw[2] -> IsoCPUQueue(200) -> [2]output;
    sw[3] -> IsoCPUQueue(200) -> [3]output;
};

elementclass Router4PortFastPathWithQueue {
    $local_addr, $bucket_size, $hash_offset |

    // $local_addr: the full address of the node
    // $hash_offset: key location

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3

    n :: RouteEngine($local_addr);
    //cache :: XIATransport($local_addr, n/proc/rt_CID/rt);
    //cache :: Queue(200);
    fp :: XIAFastPath(BUCKET_SIZE $bucket_size, KEY_OFFSET $HASH_OFFSET);

    input[0] -> [0]fp; 
    input[1] -> [0]fp;
    input[2] -> [0]fp;
    input[3] -> [0]fp;

    fp[0] // -> XIAPrint("SlowPath") 
    -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    //n[2] -> [0]cache[0] -> [1]n;
    //Idle -> [1]cache[1] -> Discard;
    Idle -> [1]n;
    //n[2] -> cache -> Unqueue -> Discard;
    n[2]-> Discard;

    sw[0] -> [1]fp[1] -> IsoCPUQueue(200) -> [0]output;
    sw[1] -> [2]fp[2] -> IsoCPUQueue(200) -> [1]output;
    sw[2] -> [3]fp[3] -> IsoCPUQueue(200) -> [2]output;
    sw[3] -> [4]fp[4] -> IsoCPUQueue(200) -> [3]output;
};


elementclass Router4PortFastPath {
    $local_addr |

    // $local_addr: the full address of the node

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3

    n :: RouteEngine($local_addr);
    //cache :: XIATransport($local_addr, n/proc/rt_CID/rt);
    //cache :: Queue(200);
    fp :: XIAFastPath(BUCKET_SIZE 1024, KEY_OFFSET 0);

    input[0] -> [0]fp; 
    input[1] -> [0]fp;
    input[2] -> [0]fp;
    input[3] -> [0]fp;

    fp[0] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    //n[2] -> [0]cache[0] -> [1]n;
    //Idle -> [1]cache[1] -> Discard;
    Idle -> [1]n;
    //n[2] -> cache -> Unqueue -> Discard;
    n[2]-> Discard;

    sw[0] -> [1]fp[1] -> [0]output;
    sw[1] -> [2]fp[2] -> [1]output;
    sw[2] -> [3]fp[3] -> [2]output;
    sw[3] -> [4]fp[4] -> [3]output;
};

// 4-port router node with "dummy cache" (for microbench)
elementclass Router4PortDummyCacheNoQueue {
    $local_addr |

    // $local_addr: the full address of the node

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3

    n :: RouteEngine($local_addr);
    //cache :: XIATransport($local_addr, n/proc/rt_CID/rt);
    //cache :: Queue(200);

    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path

    input[0] -> [0]n;
    input[1] -> [0]n;
    input[2] -> [0]n;
    input[3] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    //n[2] -> [0]cache[0] -> [1]n;
    //Idle -> [1]cache[1] -> Discard;
    Idle -> [1]n;
    //n[2] -> cache -> Unqueue -> Discard;
    n[2]-> Discard;

    sw[0] -> [0]output;
    sw[1] -> [1]output;
    sw[2] -> [2]output;
    sw[3] -> [3]output;
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
    //cache :: Queue(200);

    input[0] -> [0]n;
    input[1] -> [0]n;
    input[2] -> [0]n;
    input[3] -> [0]n;

    n[0] -> sw :: PaintSwitch
    n[1] -> Discard;
    //n[2] -> [0]cache[0] -> [1]n;
    //Idle -> [1]cache[1] -> Discard;
    Idle -> [1]n;
    //n[2] -> cache -> Unqueue -> Discard;
    n[2]-> Discard;

    sw[0] -> IsoCPUQueue(200) -> [0]output;
    sw[1] -> IsoCPUQueue(200) -> [1]output;
    sw[2] -> IsoCPUQueue(200) -> [2]output;
    sw[3] -> IsoCPUQueue(200) -> [3]output;
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

