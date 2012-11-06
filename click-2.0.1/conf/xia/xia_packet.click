define ($LENGTH 500, $OFFSET 0);

XIAXIDInfo(
    HID0 HID:0000000000000000000000000000000000000000,
    HID1 HID:1111111111111111111111111111111111111111,
    HID2 HID:2222222222222222222222222222222222222222,
    SID0  SID:2425222222222222222222222222222222222223,
    CID0 CID:000102030405060708090a0b0c0d0e0f10111213,
);


// Generate CID Response
RandomSource(LENGTH $LENGTH, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
//    SRC DAG     0 -
//        HID0,
//    DST DAG     1 -
//        HID1,
    SRC RE HID0,
    DST RE ( HID1 HID2 ) CID0,
    EXT_C_PACKET_OFFSET $OFFSET,
    EXT_C_CHUNK_OFFSET 0,
    EXT_C_CHUNK_LENGTH $LENGTH,
    EXT_C_CONTENT_LENGTH $LENGTH,
    )
-> print::XIAPrint(LENGTH true)
-> AggregateCounter(COUNT_STOP 10)
-> Discard;

// Generate CID Request
RandomSource(LENGTH $LENGTH, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
//    SRC DAG     0 -
//        HID0,
//    DST DAG     1 -
//        HID1,
    SRC RE HID0,
    DST RE ( HID1 HID2 ) CID0,
    EXT_C_REQUEST true
    )
-> print


// Generate SID packet
RandomSource(LENGTH $LENGTH, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
//    SRC DAG     0 -
//        HID0,
//    DST DAG     1 -
//        HID1,
    SRC RE HID0,
    DST RE ( HID1 HID2 ) SID0,
    )
-> print
