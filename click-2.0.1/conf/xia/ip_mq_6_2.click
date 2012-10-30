#!/usr/local/sbin/click-install -uct12
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
define ($PAYLOAD_SIZE 44);
//define ($PAYLOAD_SIZE 228);
//define ($PAYLOAD_SIZE 484);

elementclass gen_sub {
    $eth_from, $eth_to, $queue, $cpu |

    //pd1 :: MQPollDevice($eth_from, QUEUE $queue, PROMISC true) -> Discard;
    //StaticThreadSched(pd1 $cpu);

    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 12)
    //-> Script(TYPE PACKET, write gen1.active false)       // stop source after exactly 1 packet
    //-> unq :: Unqueue()
    //-> DynamicUDPIPEncap($eth_from, $SRC_PORT, $eth_to, $DST_PORT, INTERVAL 1, CHANGE_IP 1)
    -> DynamicIPEncap(4, $eth_from, $eth_to)
    -> EtherEncap(0x0800, $SRC_MAC1 , $DST_MAC1)
    -> CheckIPHeader(14)
    -> IPPrint($eth_from)
    -> clone1 ::Clone($COUNT, SHARED_SKBS true)
    -> td1 :: MQToDevice($eth_from, QUEUE $queue, BURST $BURST);
    //StaticThreadSched(gen1 $cpu, unq $cpu, td1 $cpu);
    StaticThreadSched(gen1 $cpu,  td1 $cpu);


    Script(write gen1.active true);
}

elementclass gen0 {
    $eth_from, $eth_to |

    gen_sub($eth_from, $eth_to, 0, 0);
    gen_sub($eth_from, $eth_to, 1, 1);
    gen_sub($eth_from, $eth_to, 2, 2);
    gen_sub($eth_from, $eth_to, 3, 3);
    gen_sub($eth_from, $eth_to, 4, 4);
    gen_sub($eth_from, $eth_to, 5, 5);
    gen_sub($eth_from, $eth_to, 6, 6);
    gen_sub($eth_from, $eth_to, 7, 7);
    gen_sub($eth_from, $eth_to, 8, 8);
    gen_sub($eth_from, $eth_to, 9, 9);
    gen_sub($eth_from, $eth_to, 10, 10);
    gen_sub($eth_from, $eth_to, 11, 11);
}

elementclass gen1 {
    $eth_from, $eth_to |

    gen_sub($eth_from, $eth_to, 0, 12);
    gen_sub($eth_from, $eth_to, 1, 13);
    gen_sub($eth_from, $eth_to, 2, 14);
    gen_sub($eth_from, $eth_to, 3, 15);
    gen_sub($eth_from, $eth_to, 4, 16);
    gen_sub($eth_from, $eth_to, 5, 17);
    gen_sub($eth_from, $eth_to, 6, 18);
    gen_sub($eth_from, $eth_to, 7, 19);
    gen_sub($eth_from, $eth_to, 8, 20);
    gen_sub($eth_from, $eth_to, 9, 21);
    gen_sub($eth_from, $eth_to, 10, 22);
    gen_sub($eth_from, $eth_to, 11, 23);
}

elementclass gen_b {
    $eth_from, $eth_to |

    gen_sub($eth_from, $eth_to, 0, 0);
    gen_sub($eth_from, $eth_to, 1, 1);
    gen_sub($eth_from, $eth_to, 2, 2);
    gen_sub($eth_from, $eth_to, 3, 3);
    gen_sub($eth_from, $eth_to, 4, 4);
    gen_sub($eth_from, $eth_to, 5, 5);
    gen_sub($eth_from, $eth_to, 6, 6);
    gen_sub($eth_from, $eth_to, 7, 7);
    gen_sub($eth_from, $eth_to, 8, 8);
    gen_sub($eth_from, $eth_to, 9, 9);
    gen_sub($eth_from, $eth_to, 10, 10);
    gen_sub($eth_from, $eth_to, 11, 11);
    gen_sub($eth_from, $eth_to, 12, 12);
    gen_sub($eth_from, $eth_to, 13, 13);
    gen_sub($eth_from, $eth_to, 14, 14);
    gen_sub($eth_from, $eth_to, 15, 15);
    gen_sub($eth_from, $eth_to, 16, 16);
    gen_sub($eth_from, $eth_to, 17, 17);
    gen_sub($eth_from, $eth_to, 18, 18);
    gen_sub($eth_from, $eth_to, 19, 19);
    gen_sub($eth_from, $eth_to, 20, 20);
    gen_sub($eth_from, $eth_to, 21, 21);
    gen_sub($eth_from, $eth_to, 22, 22);
    gen_sub($eth_from, $eth_to, 23, 23);
}

//gen_b(eth2, eth4);
//gen_b(eth3, eth5);
//gen_b(eth4, eth2);
//gen_b(eth5, eth3);

//gen0(eth2, eth4);
//gen1(eth3, eth5);
//gen0(eth4, eth2);
//gen1(eth5, eth3);

gen_sub(eth2, eth3, 0, 0);
gen_sub(eth2, eth3, 1, 2);
gen_sub(eth2, eth3, 2, 4);
gen_sub(eth2, eth3, 3, 6);
gen_sub(eth2, eth3, 4, 8);
gen_sub(eth2, eth3, 5, 10);

gen_sub(eth3, eth2, 0, 1);
gen_sub(eth3, eth2, 1, 3);
gen_sub(eth3, eth2, 2, 5);
gen_sub(eth3, eth2, 3, 7);
gen_sub(eth3, eth2, 4, 9);
gen_sub(eth3, eth2, 5, 11);

