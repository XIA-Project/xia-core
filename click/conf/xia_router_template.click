// aliases for XIDs
XIDInfo(
    HID0 HID:0000000000000000000000000000000000000000,
    HID1 HID:0000000000000000000000000000000000000001,
    AD0 AD:1000000000000000000000000000000000000000,
    AD1 AD:1000000000000000000000000000000000000001,
);

// dumb destination processor
elementclass Destination {
    $id |
    input -> Print("packet received by $id") -> XIAPrint(HLIM true)
    -> Discard;
};

// host
elementclass Host {
    $self_addr |
    c :: XIDTypeClassifier(src HID, -);
    rt :: StaticXIDLookup($self_addr 0, dest 1, - 2);
    input -> c;
    c[0] -> rt;
    c[1] -> Print("unrecognized source XID type") -> Discard;
    rt[0] -> XIANextHop -> rt;
    rt[1] -> [1]output;                                 // to the upper level
    rt[2] -> DecXIAHLIM -> Queue(200) -> [0]output;     // forward to the other node
};

// router 
elementclass Router {
    $self_addr, $local_host_addr |
    c :: XIDTypeClassifier(src HID, -);
    input[0] -> c;
    input[1] -> c;
    rt :: StaticXIDLookup($self_addr 0, $local_host_addr 1, - 2);
    c[0] -> rt;
    c[1] -> Print("unrecognized source XID type") -> Discard;
    rt[0] -> XIANextHop -> rt;
    rt[1] -> DecXIAHLIM -> Queue(200) -> [0]output;     // forward within the local domain
    rt[2] -> DecXIAHLIM -> Queue(200) -> [1]output;     // forward to the other domain
};

// host & router instantiation
host0 :: Host(HID0);
host1 :: Host(HID1);
router0 :: Router(AD0, HID0);
router1 :: Router(AD1, HID1);

// interconnection -- destination
host0[1] -> Destination(host0);
host1[1] -> Destination(host1);

// interconnection -- host - ad
host0[0] -> Unqueue -> [0]router0;
router0[0] -> Unqueue -> [0]host0;

host1[0] -> Unqueue -> [0]router1;
router1[0] -> Unqueue -> [0]host1;

// interconnection -- ad - ad
router0[1] -> Unqueue -> [1]router1;
router1[1] -> Unqueue -> [1]router0;


// send test packets from host0 to host1
RandomSource(LENGTH 100, HEADROOM 256)
-> XIAEncap(
    NXT 0,
    DST RE AD1 HID1,
    SRC RE AD0 HID0)
-> AggregateCounter(COUNT_STOP 1)
-> host0;

// send test packets from host1 to host0
//RandomSource(LENGTH 100, HEADROOM 256)
//-> XIAEncap(
//    NXT 0,
//    DST RE AD0 HID0,
//    SRC RE AD1 HID1)
//-> AggregateCounter(COUNT_STOP 1)
//-> host1;

