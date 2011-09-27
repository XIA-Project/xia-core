require(library xia_packetforward_intra.click);


gen :: InfiniteSource(LENGTH $PAYLOAD_SIZE_XIA_XID2, ACTIVE false, HEADROOM $HEADROOM_SIZE_XIA_XID2)
-> XIAEncap(SRC RE UNROUTABLE_AD0, DST RE ( RANDOM_ID ) ARB_RANDOM_ID)
->SerialPath
Script(write gen.active true);

