#!/usr/local/sbin/click-install -uct1
define ($IP_PKT_SIZE 64);
define ($DST_MAC 00:15:17:51:d3:d4);
define ($HEADROOM_SIZE 256);
define ($BURST 64);
define ($SRC_PORT 5012);
define ($DST_PORT 5002);
define ($PKT_COUNT 500000000);
define ($SRC_MAC 00:1a:92:9b:4a:77);
define ($COUNT 250000000);
define ($PAYLOAD_SIZE 36);


//MQPollDevice(eth2) -> Discard;
//MQPollDevice(eth3) -> Discard;
//MQPollDevice(eth4) -> Discard;
//MQPollDevice(eth5) -> Discard;



gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen1.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(eth2, $SRC_PORT, 10.0.2.121, $DST_PORT)
-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
-> CheckIPHeader(14)
-> clone1 ::Clone($COUNT)
-> RatedUnqueue(10)
-> Queue()
-> IPPrint(gen1_eth2)
//-> td1 :: MQToDevice(eth2, QUEUE 0, BURST $BURST);
-> td1 :: ToDevice(eth2);
//StaticThreadSched(td1 0, clone1 0);



//gen2:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
//-> Script(TYPE PACKET, write gen2.active false)       // stop source after exactly 1 packet
//-> Unqueue()
//-> UDPIPEncap(eth2, $SRC_PORT, eth4, $DST_PORT)
//-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)
//-> CheckIPHeader(14)
//-> clone2 ::Clone($COUNT)
//-> RatedUnqueue(1)
//-> Queue()
//-> IPPrint(gen2_eth2)
//-> td2 :: MQToDevice(eth2, QUEUE 1, BURST $BURST);
//StaticThreadSched(td2 1, clone2 1);

Script(write gen1.active true);
//Script(write gen2.active true);
