// aliases for XIDs
XIDInfo(
    HID0 HID:0000000000000000000000000000000000000000,
    HID1 HID:0000000000000000000000000000000000000001,
    AD0 AD:1000000000000000000000000000000000000000,
    AD1 AD:1000000000000000000000000000000000000001,
)

// dumb userlevel processor
elementclass Userlevel {
    $id |
    input -> Print("packet received by $id") -> XIAPrint()
    -> AggregateCounter(COUNT_STOP 1)
    -> Discard;
};

// host
elementclass Host {
    $hid |
    c :: SrcXIDTypeClassifier(HID, -);
    rt :: StaticXIDLookup($hid 0, - 1);
    input -> c;
    c[0] -> rt;
    c[1] -> Print("unrecognized XID type") -> Discard;
    rt[0] -> [1]output;
    rt[1] -> Queue(200) -> [0]output; 
};

// router 
elementclass Router {
    $ad0, $ad1 |
    c :: SrcXIDTypeClassifier(AD, -);
    rt :: StaticXIDLookup($ad0 0, $ad1 1, - 2);
    input[0] -> c;
    input[1] -> c;
    c[0] -> rt;
    c[1] -> Print("unrecognized XID type") -> Discard;
    rt[0] -> Queue(200) -> [0]output; 
    rt[1] -> Queue(200) -> [1]output; 
    rt[2] -> Print("unroutable XID") -> Discard;
};

// host & router instantiation
host0 :: Host(HID0);
host1 :: Host(HID1);
router :: Router(AD0, AD1);

// interconnection
host0[0] -> Unqueue -> [0]router;
router[0] -> Unqueue -> [0]host0;

host1[0] -> Unqueue -> [1]router;
router[1] -> Unqueue -> [0]host1;

host0[1] -> Userlevel(0);
host1[1] -> Userlevel(1);


// send test packets from host0 to host1 
RandomSource(LENGTH 100, HEADROOM 256)
-> XIAEncap(
    NXT 0,
    DST RE HID1,
    SRC RE HID0)
-> host0;

