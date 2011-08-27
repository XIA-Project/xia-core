#!/usr/local/sbin/click-install -uct24
require(library xia_address.click);

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
define ($PAYLOAD_SIZE 36);
//define ($PAYLOAD_SIZE 228);
//define ($PAYLOAD_SIZE 484);

elementclass gen_sub {
    $hid_from, $hid_to, $eth_from, $eth_to, $queue, $cpu |

    pd1 :: MQPollDevice($eth_from, QUEUE $queue, PROMISC true) -> Discard;
    StaticThreadSched(pd1 $cpu);

    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 240)
    -> XIAEncap(
           DST RE  $hid_from,
           SRC RE  $hid_to)
    -> XIAPrint($eth_from)
    -> DynamicIPEncap(0x99, $eth_from, $eth_to)
    -> EtherEncap(0x0800, $SRC_MAC1 , $DST_MAC1)
    -> CheckIPHeader(14)
    -> IPPrint($eth_from)
    -> clone1 ::Clone($COUNT, SHARED_SKBS true)
    -> td1 :: MQToDevice($eth_from, QUEUE $queue, BURST $BURST);
    StaticThreadSched(gen1 $cpu,  td1 $cpu);

    Script(write gen1.active true);
}

elementclass gen0 {
    $hid_from, $hid_to, $eth_from, $eth_to |

    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 0, 0);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 1, 1);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 2, 2);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 3, 3);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 4, 4);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 5, 5);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 6, 6);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 7, 7);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 8, 8);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 9, 9);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 10, 10);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 11, 11);
}

elementclass gen1 {
    $hid_from, $hid_to, $eth_from, $eth_to |

    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 0, 12);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 1, 13);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 2, 14);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 3, 15);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 4, 16);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 5, 17);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 6, 18);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 7, 19);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 8, 20);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 9, 21);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 10, 22);
    gen_sub($hid_from, $hid_to, $eth_from, $eth_to, 11, 23);
}

gen0(HID2, HID4, eth2, eth4);
gen1(HID3, HID5, eth3, eth5);
gen0(HID4, HID2, eth4, eth2);
gen1(HID5, HID3, eth5, eth3);

//gen_sub(eth2, eth4, 0, 0);
//gen_sub(eth4, eth2, 0, 1);
//gen_sub(eth3, eth5, 0, 2);
//gen_sub(eth5, eth3, 0, 3);

