define ($DST_MAC 00:15:17:51:d3:d4);
define ($DST_MAC1 00:25:17:51:d3:d4);
define ($HEADROOM_SIZE 256);
define ($BURST 32);
define ($SRC_PORT 5012);
define ($DST_PORT 5002);
define ($PKT_COUNT 5000000000);
define ($SRC_MAC 00:1a:92:9b:4a:77);
define ($SRC_MAC1 00:1a:92:9b:4a:71);
define ($COUNT 1000000000);
define ($PAYLOAD_SIZE 30);
//define ($PAYLOAD_SIZE 228);
//define ($PAYLOAD_SIZE 484);

elementclass gen_sub {
    $eth_from, $eth_to, $queue, $cpu |

    //pd1 :: MQPollDevice($eth_from, QUEUE $queue, PROMISC true) -> Discard;
    //StaticThreadSched(pd1 $cpu);

    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 240)
    //-> Script(TYPE PACKET, write gen1.active false)       // stop source after exactly 1 packet
    //-> unq :: Unqueue()
    //-> DynamicUDPIPEncap($eth_from, $SRC_PORT, $eth_to, $DST_PORT, INTERVAL 1, CHANGE_IP 1)
    //-> DynamicIPEncap(4, $eth_from, $eth_to)
    -> IPEncap(0x90, 10.0.0.1, 10.0.2.15)
    -> EtherEncap(0x0800, $SRC_MAC1 , $DST_MAC1)
    -> CheckIPHeader(14)
    -> IPPrint($eth_from)
    //-> clone1 ::Clone($COUNT, SHARED_SKBS true)
    -> td1 :: MQToDevice($eth_from, QUEUE $queue, BURST $BURST);
    //StaticThreadSched(gen1 $cpu, unq $cpu, td1 $cpu);
    StaticThreadSched(gen1 $cpu,  td1 $cpu);


    Script(write gen1.active true);
}

elementclass gen0 {
    $eth_from, $eth_to |

    gen_sub($eth_from, $eth_to, 0, 0);
}

gen0(xge0, xge2);
//gen0(eth3, eth5);
//gen0(eth4, eth2);
//gen0(eth5, eth3);
