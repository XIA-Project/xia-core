require(library xia_packetforward_intra.click);

gen :: InfiniteSource(LENGTH $PAYLOAD_SIZE_XIA_XID2, ACTIVE false, HEADROOM $HEADROOM_SIZE_XIA_XID2)
//-> Script(TYPE PACKET, write gen.active false)       // stop source after exactly 1 packet
-> XIAEncap(SRC RE UNROUTABLE_AD0, DST RE RANDOM_ID)
-> Intra(gen)

//Script(write gen.active true);

