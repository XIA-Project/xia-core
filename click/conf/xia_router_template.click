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
    $hid |
    c :: SrcXIDTypeClassifier(HID, -);
    rt :: StaticXIDLookup($hid 0, - 1, dest 2);
    input -> c;
    c[0] -> rt;
    c[1] -> Print("unrecognized XID type") -> Discard;
    rt[0] -> XIANextHop -> rt;
    rt[1] -> DecXIAHLIM -> Queue(200) -> [0]output; 
    rt[2] -> [1]output;
};

// router 
elementclass Router {
    $hid0, $hid1 |
    c :: SrcXIDTypeClassifier(HID, -);
    input[0] -> c;
    input[1] -> c;
    rt :: StaticXIDLookup($hid0 0, $hid1 1, - 2);
    c[0] -> rt;
    c[1] -> Print("unrecognized XID type") -> Discard;
    rt[0] -> DecXIAHLIM -> Queue(200) -> [0]output; 
    rt[1] -> DecXIAHLIM -> Queue(200) -> [1]output; 
    rt[2] -> Print("unroutable XID") -> Discard;
};

// host & router instantiation
host0 :: Host(HID0);
host1 :: Host(HID1);
router :: Router(HID0, HID1);

// interconnection
host0[0] -> Unqueue -> [0]router;
router[0] -> Unqueue -> [0]host0;

host1[0] -> Unqueue -> [1]router;
router[1] -> Unqueue -> [0]host1;

host0[1] -> Destination(host0);
host1[1] -> Destination(host1);

// send test packets from host0 to host1
RandomSource(LENGTH 100, HEADROOM 256)
-> XIAEncap(
    NXT 0,
    DST RE HID1,
    SRC RE HID0)
-> AggregateCounter(COUNT_STOP 1)
-> host0;

// send test packets from host1 to host0
//RandomSource(LENGTH 100, HEADROOM 256)
//-> XIAEncap(
//    NXT 0,
//    DST RE HID0,
//    SRC RE HID1)
//-> AggregateCounter(COUNT_STOP 1)
//-> host1;

