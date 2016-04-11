require(library xia_packetforward_intra.click);

gen :: InfiniteSource(LENGTH $PAYLOAD_SIZE_XIA_XID2, ACTIVE false, HEADROOM $HEADROOM_SIZE_XIA_XID2)
//-> Script(TYPE PACKET, write gen.active false)       // stop source after exactly 1 packet
-> XIAEncap(SRC RE UNROUTABLE_AD0,
            DST DAG               2 1 0 -   // -1
                    RANDOM_ID     2 -       //  0 
                    ARB_RANDOM_ID 2 -       //  1
                    ARB_RANDOM_ID           //  2
)
-> Intra(gen)

//Script(write gen.active true);

