require(library ../xia_router_template.inc);
require(library common.inc);


gen :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen.active false)       // stop source after exactly 1 packet
-> IP6Encap(9, UNROUTABLE_IP6, RANDOM_IP6)
-> fwd :: IP6PacketForward;

Script(write gen.active true);

