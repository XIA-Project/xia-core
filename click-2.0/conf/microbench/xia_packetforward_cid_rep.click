require(library ../xia_router_template.inc);
require(library common.inc);


gen :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen.active false)       // stop source after exactly 1 packet
-> XIAEncap(SRC RE ( UNROUTABLE_AD0 ) CID0, DST RE RANDOM_ID,
            EXT_C_PACKET_OFFSET 0,
            EXT_C_CHUNK_OFFSET 0,
            EXT_C_CHUNK_LENGTH $PAYLOAD_SIZE,
            EXT_C_CONTENT_LENGTH $PAYLOAD_SIZE)
-> fwd :: XIAPacketForward;

Idle -> [1]fwd;
Idle -> [2]fwd;

Script(write gen.active true);

define($AD_RT_SIZE 351611);
define($CID_RT_SIZE 10);

