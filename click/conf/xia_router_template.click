// aliases for XIDs
XIAXIDInfo(
    HID0 HID:0000000000000000000000000000000000000000,
    HID1 HID:0000000000000000000000000000000000000001,
    AD0 AD:1000000000000000000000000000000000000000,
    AD1 AD:1000000000000000000000000000000000000001,
    CID0 CID:2000000000000000000000000000000000000001,
);

elementclass Destination {
    $name |
    input -> Print("packet received by $name") -> XIAPrint(HLIM true) -> Discard;
};


elementclass ContentCache {
    // input: a packet
    // output: a packet (passthru)

    input -> c :: XIAXIDTypeClassifier(src CID, -);

    c[0] -> dup :: Tee(2);
    c[1] -> output;

    dup[0] -> output;
    dup[1] -> store_to_cache :: Discard;
};

elementclass GenericRouting {
    // input: a packet to route
    // output[0]: forward to port 0
    // output[1]: forward to port 1
    // output[2]: forward to port 2
    // output[3]: forward to port 3
    // output[4]: need to update "last" pointer
    // output[5]: failed to route

    input -> rt :: XIAXIDRouteTable;
    rt[0] -> [0]output;     // forward to port 0
    rt[1] -> [1]output;     // forward to port 1
    rt[2] -> [2]output;     // forward to port 2
    rt[3] -> [3]output;     // forward to port 3
    rt[4] -> [4]output;     // update last pointer
    rt[5] -> [5]output;     // failed to route
};

elementclass PerHopProc {
    // input: a packet to process
    // output[0]: forward to port 0
    // output[1]: forward to port 1
    // output[2]: forward to port 2
    // output[3]: forward to port 3
    // output[4]: arrived at destination node
    // output[5]: failed to route

    check_dest :: XIACheckDest();
    consider_first_path :: XIASelectPath(first);
    consider_next_path :: XIASelectPath(next);
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, -);

    input -> check_dest;

    check_dest[0] -> [4]output;
    check_dest[1] -> consider_first_path;

    consider_first_path[0] -> c;
    consider_first_path[1] -> [5]output;
    consider_next_path[0] -> c;
    consider_next_path[1] -> [5]output;

    c[0] -> rt_AD :: GenericRouting;
    rt_AD[0] -> XIADecHLIM -> [0]output;
    rt_AD[1] -> XIADecHLIM -> [1]output;
    rt_AD[2] -> XIADecHLIM -> [2]output;
    rt_AD[3] -> XIADecHLIM -> [3]output;
    rt_AD[4] -> XIANextHop -> check_dest;
    rt_AD[5] -> consider_next_path;

    c[1] -> rt_HID :: GenericRouting;
    rt_HID[0] -> XIADecHLIM -> [0]output;
    rt_HID[1] -> XIADecHLIM -> [1]output;
    rt_HID[2] -> XIADecHLIM -> [2]output;
    rt_HID[3] -> XIADecHLIM -> [3]output;
    rt_HID[4] -> XIANextHop -> check_dest;
    rt_HID[5] -> consider_next_path;

    c[2] -> rt_SID :: GenericRouting;
    rt_SID[0] -> XIADecHLIM -> [0]output;
    rt_SID[1] -> XIADecHLIM -> [1]output;
    rt_SID[2] -> XIADecHLIM -> [2]output;
    rt_SID[3] -> XIADecHLIM -> [3]output;
    rt_SID[4] -> XIANextHop -> check_dest;
    rt_SID[5] -> consider_next_path;

    c[3] -> rt_CID :: GenericRouting;
    rt_CID[0] -> XIADecHLIM -> [0]output;
    rt_CID[1] -> XIADecHLIM -> [1]output;
    rt_CID[2] -> XIADecHLIM -> [2]output;
    rt_CID[3] -> XIADecHLIM -> [3]output;
    rt_CID[4] -> XIANextHop -> check_dest;
    rt_CID[5] -> consider_next_path;

    c[4] -> [5]output;
};

elementclass CachingNode {
    // input: a packet arrived at a node 
    // output[0]: forward to port 0
    // output[1]: forward to port 1
    // output[2]: forward to port 2
    // output[3]: forward to port 3
    // output[4]: arrived at destination node

    input -> ContentCache -> proc :: PerHopProc;

    proc[0] -> Queue(200) -> [0]output;
    proc[1] -> Queue(200) -> [1]output;
    proc[2] -> Queue(200) -> [2]output;
    proc[3] -> Queue(200) -> [3]output;
    proc[4] -> [4]output;
    proc[5] -> Discard;
};

// 1-port host node
elementclass Host {
    $hid |

    // input: a packet arrived at a node 
    // output[0]: forward to port 0

    n :: CachingNode;

    Script(write n/proc/rt_AD/rt.add - 0);
    Script(write n/proc/rt_HID/rt.add - 0);
    Script(write n/proc/rt_HID/rt.add $hid 4);
    Script(write n/proc/rt_SID/rt.add - 5);
    Script(write n/proc/rt_CID/rt.add - 5);

    input -> n;
    n[0] -> output;
    n[1] -> Discard;
    n[2] -> Discard;
    n[3] -> Discard;
    n[4] -> Destination($hid);
};

// 2-port router node
elementclass Router {
    $ad, $hid |

    // input: a packet arrived at a node 
    // output[0]: forward to port 0 (for $hid)
    // output[1]: forward to port 1 (for other ads)

    n :: CachingNode;
    
    Script(write n/proc/rt_AD/rt.add - 1);
    Script(write n/proc/rt_AD/rt.add $ad 4);
    Script(write n/proc/rt_HID/rt.add $hid 0);
    Script(write n/proc/rt_SID/rt.add - 5);
    Script(write n/proc/rt_CID/rt.add - 5);

    input[0] -> n;
    input[1] -> n;
    n[0] -> [0]output;
    n[1] -> [1]output;
    n[2] -> Discard;
    n[3] -> Discard;
    n[4] -> Discard;
};

// host & router instantiation
host0 :: Host(HID0);
host1 :: Host(HID1);
router0 :: Router(AD0, HID0);
router1 :: Router(AD1, HID1);

// interconnection -- host - ad
host0[0] -> Unqueue -> [0]router0;
router0[0] -> Unqueue -> [0]host0;

host1[0] -> Unqueue -> [0]router1;
router1[0] -> Unqueue -> [0]host1;

// interconnection -- ad - ad
router0[1] -> Unqueue -> [1]router1;
router1[1] -> Unqueue -> [1]router0;

// send test packets from host0 to host1
gen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
-> XIAEncap(
    NXT 0,
    DST RE AD1 HID1,
    SRC RE AD0 HID0)
-> AggregateCounter(COUNT_STOP 1)
-> host0;

// send test packets from host1 to host0
//gen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
//-> XIAEncap(
//    NXT 0,
//    DST RE AD0 HID0,
//    SRC RE AD1 HID1)
//-> AggregateCounter(COUNT_STOP 1)
//-> host1;

Script(write gen.active true);

