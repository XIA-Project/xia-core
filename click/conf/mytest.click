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
}

elementclass XIAPacketRoute {
    $local_addr |
    // input: a packet to process
    // output[0]: forward (painted)
    // output[1]: arrived at destination node
    // output[2]: could not route at all (tried all paths)

    check_dest :: XIACheckDest();
    consider_first_path :: XIASelectPath(first);
    consider_next_path :: XIASelectPath(next);
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, -);

    //input -> Print("packet received by $local_addr") -> check_dest;
    input -> check_dest;

    check_dest[0] -> [1]output; // arrived at the final destination
    check_dest[1] -> consider_first_path;

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
    CIDPostRouteProc :: Null ; 

    //  Next destination is CID
    c[3] -> rt_CID :: GenericRouting4Port;
    rt_CID[0] -> GenericPostRouteProc -> CIDPostRouteProc -> [0]output;
    rt_CID[1] -> XIANextHop -> check_dest;
    rt_CID[2] -> consider_next_path;

    c[4] -> [2]output;
};

elementclass RouteEngine {
    $local_addr |
    // input[0]: a packet arrived at a node from outside (i.e. routing with caching)
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

    proc[2] -> Discard;  // No route drop (future TODO: return an error packet)
};

// 1-port host node
elementclass Host {
    $local_addr, $local_hid, $rpc_port |

    // input: a packet arrived at a node 
    // output: forward to interface 0

    n :: RouteEngine($hid);
    sock :: Socket(TCP, 0.0.0.0, $rpc_port, CLIENT false);
    rpc :: XIARPC($local_addr);
    cache :: XIARouterCache($local_addr, n/proc/rt_CID/rt);

    Script(write n/proc/rt_AD/rt.add - 0);      // default route for AD
    Script(write n/proc/rt_HID/rt.add - 0);     // default route for HID
    Script(write n/proc/rt_HID/rt.add $local_hid 4);  // self HID as destination
    Script(write n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path

    input[0] -> n;

    sock -> [0]rpc[0] -> sock;
    n[1] -> [1]rpc[1] -> [0]n;
    n[2] -> [0]cache[0] -> [1]n;
    rpc[2] -> Print("Push CID")-> [1]cache[1] -> [2]rpc;

    n -> Queue(200) -> [0]output;
};

// 2-port router node
elementclass Router {
    $local_addr, $local_ad, $local_hid |

    // input[0], input[1]: a packet arrived at a node (regardless of interface)
    // output[0]: forward to interface 0 (for hosts in local ad)
    // output[1]: forward to interface 1 (for other ads)

    n :: RouteEngine($ad);
    cache :: XIARouterCache($local_addr, n/proc/rt_CID/rt);
    
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




XIAXIDInfo(
    HID0 HID:0000000000000000000000000000000000000000,
    HID1 HID:0000000000000000000000000000000000000001,
    AD0 AD:1000000000000000000000000000000000000000,
    AD1 AD:1000000000000000000000000000000000000001,
    RHID0 HID:0000000000000000000000000000000000000002,
    RHID1 HID:0000000000000000000000000000000000000003,
    CID0 CID:0000000000000000000000000000000000000000,
    CID1 CID:2000000000000000000000000000000000000002,
);

//print::XIAPrint(LENGTH true)

//Idle->rt :: XIAXIDRouteTable->Discard
//cache::XIARouterCache(RE AD0 HID0, rt)

//router::Router(RE AD0 RHID0, AD0, RHID0)
//Idle->[1]router



//host0->Print("host output:")->print->Discard;
//Idle->host0;

host0 :: Host(RE AD0 HID0, HID0, 2000);
router0 :: Router(RE AD0 RHID0, AD0, RHID0);
router1 :: Router(RE AD1 RHID1, AD1, RHID1);
host1:: Host(RE AD1 HID1, HID1, 2001);

host0[0] -> Unqueue -> Print("(1)")->XIAPrint()->[0]router0;
router0[0] -> Unqueue -> Print("(6)")->XIAPrint()->[0]host0;

router0[1] -> Unqueue -> Print("(2)")->XIAPrint()-> [1]router1;
router1[1] -> Unqueue -> Print("(5)")->XIAPrint()->[1]router0;

router1[0]-> Unqueue -> Print("(3)")->XIAPrint()-> host1
host1-> Unqueue ->Print("(4)")->XIAPrint()->[0]router1

//Idle->[0]router1
//router1[0]->Print("Router1 output")->print->Discard;

/*
genReq :: InfiniteSource(LENGTH 500, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(1)
-> XIAEncap(
    DST RE CID0,
    SRC RE AD0 HID0,
    )
->Print("input request")->XIAPrint(LENGTH true)
->host1


host1->Print("host output")->print->Discard;


Script(write genReq.active true)
*/


/*
gen0 :: InfiniteSource(LENGTH 500, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(1)
-> XIAEncap(
    DST RE HID0,
    SRC RE ( HID1 HID0 ) CID0,
    EXT_C_PACKET_OFFSET 0,
    EXT_C_CHUNK_OFFSET 1000,
    EXT_C_CHUNK_LENGTH 1500,
    EXT_C_CONTENT_LENGTH 500,
    )
-> cache

gen1 :: InfiniteSource(LENGTH 500, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(1)
-> XIAEncap(
    DST RE HID0,
    SRC RE ( HID1 HID0 ) CID0,
    EXT_C_PACKET_OFFSET 0,
    EXT_C_CHUNK_OFFSET 0,
    EXT_C_CHUNK_LENGTH 1500,
    EXT_C_CONTENT_LENGTH 500,
    )
-> cache

gen2::InfiniteSource(LENGTH 500, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(1)
-> XIAEncap(
    DST RE HID0,
    SRC RE ( HID1 HID0 ) CID0,
    EXT_C_PACKET_OFFSET 0,
    EXT_C_CHUNK_OFFSET 500,
    EXT_C_CHUNK_LENGTH 1500,
    EXT_C_CONTENT_LENGTH 500,
    )
-> cache


//Script(write router/n/proc/rt_HID/rt.add HID:0000000000000000000000000000000000000001 1)


gen3 :: InfiniteSource(LENGTH 500, ACTIVE false, HEADROOM 256)  //request
-> RatedUnqueue(1)
-> XIAEncap(
    SRC RE HID0,
    DST RE CID0,
    )
//->Print("request")->XIAPrint(LENGTH true)
->cache



Idle->[1]cache
cache[0]
->Print("port0 ")
->print
cache[1]
->Print("port1 ")
->print
print->Discard

*/

//router[0]-> Unqueue ->Print("port0:")->print->Discard
//router[1]-> Unqueue ->Print("port1:")->print

/*
Script(write gen0.active true)
Script(write gen1.active true)
Script(write gen2.active true)
Script(write gen3.active true)
*/
