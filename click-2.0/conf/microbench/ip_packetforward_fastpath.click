require(library ../xia_router_template.inc);
require(library common.inc);


gen :: InfiniteSource(LENGTH $PAYLOAD_SIZE_IP, ACTIVE false, HEADROOM $HEADROOM_SIZE_IP)
-> Script(TYPE PACKET, write gen.active false)       // stop source after exactly 1 packet
-> IPEncap(9, UNROUTABLE_IP, ZIPF_RANDOM_IP)
-> fwd :: IPPacketForwardFastPath;

Script(write gen.active true);

