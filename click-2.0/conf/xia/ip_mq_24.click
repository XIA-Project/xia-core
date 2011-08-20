#!/usr/local/sbin/click-install -uct24
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
define ($PAYLOAD_SIZE 36);

elementclass gen_sub {
    $eth_from, $eth_to, $queue, $cpu |

    pd1 :: MQPollDevice($eth_from, QUEUE $queue, PROMISC true) -> Discard;
    StaticThreadSched(pd1 $cpu);

    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
    -> Script(TYPE PACKET, write gen1.active false)       // stop source after exactly 1 packet
    -> Unqueue()
    -> UDPIPEncap($eth_from, $SRC_PORT, $eth_to, $DST_PORT)
    -> EtherEncap(0x0800, $SRC_MAC1 , $DST_MAC1)
    -> CheckIPHeader(14)
    -> IPPrint(gen1_eth2)
    -> clone1 ::Clone($COUNT)
    -> td1 :: MQToDevice($eth_from, QUEUE $queue, BURST $BURST);
    StaticThreadSched(td1 $cpu, clone1 $cpu);

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

gen0(eth2, eth4);
gen1(eth3, eth5);
gen0(eth4, eth2);
gen1(eth5, eth3);

//gen_sub(eth2, eth4, 0, 0);
//gen_sub(eth4, eth2, 0, 1);
//gen_sub(eth3, eth5, 0, 2);
//gen_sub(eth5, eth3, 0, 3);

