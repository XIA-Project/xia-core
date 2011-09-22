require(library ../xia_router_template.inc);
require(library common.inc);


gen :: InfiniteSource(LENGTH $PAYLOAD_SIZE_XIA_XID4, ACTIVE false, HEADROOM $HEADROOM_SIZE_XIA_XID4)
-> Script(TYPE PACKET, write gen.active false)       // stop source after exactly 1 packet
-> XIAEncap(SRC RE UNROUTABLE_AD0,
            DST DAG               2 1 0 -   // -1
                    RANDOM_ID     2 -       //  0 
                    ARB_RANDOM_ID 2 -       //  1
                    ARB_RANDOM_ID           //  2
)
-> fwd :: XIAPacketForward;

Idle -> [1]fwd;
Idle -> [2]fwd;

Script(write gen.active true);

define($AD_RT_SIZE 351611);
define($CID_RT_SIZE 10);

