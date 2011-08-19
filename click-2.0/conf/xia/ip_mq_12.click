#!/usr/local/sbin/click-install -uct12
define ($IP_PKT_SIZE 64);
define ($DST_MAC 00:15:17:51:d3:d4);
define ($DST_MAC1 00:25:17:51:d3:d4);
define ($HEADROOM_SIZE 256);
define ($BURST 64);
define ($SRC_PORT 5012);
define ($DST_PORT 5002);
define ($PKT_COUNT 5000000000);
define ($SRC_MAC 00:1a:92:9b:4a:77);
define ($SRC_MAC1 00:1a:92:9b:4a:71);
define ($COUNT 416666660);
define ($PAYLOAD_SIZE 500);


MQPollDevice(eth2) -> Discard;
MQPollDevice(eth3) -> Discard;
MQPollDevice(eth4) -> Discard;
MQPollDevice(eth5) -> Discard;



gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen1.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth2, $SRC_PORT, eth4, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC1 , $DST_MAC1)
-> CheckIPHeader(14)
-> IPPrint(gen1_eth2)
-> clone1 ::Clone($COUNT)
-> td1 :: MQToDevice(eth2, QUEUE 0, BURST $BURST);
StaticThreadSched(td1 0, clone1 0);



gen2:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen2.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth2, $SRC_PORT, eth4, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen2_eth2)
-> clone2 ::Clone($COUNT)
-> td2 :: MQToDevice(eth2, QUEUE 1, BURST $BURST);
StaticThreadSched(td2 1, clone2 1);



gen3:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen3.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth2, $SRC_PORT, eth4, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen3_eth2)
-> clone3 ::Clone($COUNT)
-> td3 :: MQToDevice(eth2, QUEUE 2, BURST $BURST);
StaticThreadSched(td3 2, clone3 2);



gen4:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen4.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth3, $SRC_PORT, eth5, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen4_eth3)
-> clone4 ::Clone($COUNT)
-> td4 :: MQToDevice(eth3, QUEUE 3, BURST $BURST);
StaticThreadSched(td4 3, clone4 3);



gen5:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen5.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth3, $SRC_PORT, eth5, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen5_eth3)
-> clone5 ::Clone($COUNT)
-> td5 :: MQToDevice(eth3, QUEUE 4, BURST $BURST);
StaticThreadSched(td5 4, clone5 4);



gen6:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen6.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth3, $SRC_PORT, eth5, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen6_eth3)
-> clone6 ::Clone($COUNT)
-> td6 :: MQToDevice(eth3, QUEUE 5, BURST $BURST);
StaticThreadSched(td6 5, clone6 5);



gen7:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen7.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth4, $SRC_PORT, eth2, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen7_eth4)
-> clone7 ::Clone($COUNT)
-> td7 :: MQToDevice(eth4, QUEUE 6, BURST $BURST);
StaticThreadSched(td7 6, clone7 6);



gen8:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen8.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth4, $SRC_PORT, eth2, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen8_eth4)
-> clone8 ::Clone($COUNT)
-> td8 :: MQToDevice(eth4, QUEUE 7, BURST $BURST);
StaticThreadSched(td8 7, clone8 7);



gen9:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen9.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth4, $SRC_PORT, eth2, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen9_eth4)
-> clone9 ::Clone($COUNT)
-> td9 :: MQToDevice(eth4, QUEUE 8, BURST $BURST);
StaticThreadSched(td9 8, clone9 8);



gen10:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen10.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth5, $SRC_PORT, eth3, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen10_eth5)
-> clone10 ::Clone($COUNT)
-> td10 :: MQToDevice(eth5, QUEUE 9, BURST $BURST);
StaticThreadSched(td10 9, clone10 9);



gen11:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen11.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth5, $SRC_PORT, eth3, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen11_eth5)
-> clone11 ::Clone($COUNT)
-> td11 :: MQToDevice(eth5, QUEUE 10, BURST $BURST);
StaticThreadSched(td11 10, clone11 10);



gen12:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen12.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth5, $SRC_PORT, eth3, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> IPPrint(gen12_eth5)
-> clone12 ::Clone($COUNT)
-> td12 :: MQToDevice(eth5, QUEUE 11, BURST $BURST);
StaticThreadSched(td12 11, clone12 11);

Script(write gen1.active true);
Script(write gen2.active true);
Script(write gen3.active true);
Script(write gen4.active true);
Script(write gen5.active true);
Script(write gen6.active true);
Script(write gen7.active true);
Script(write gen8.active true);
Script(write gen9.active true);
Script(write gen10.active true);
Script(write gen11.active true);
Script(write gen12.active true);
