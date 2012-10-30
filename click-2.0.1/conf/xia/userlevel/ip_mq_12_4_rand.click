//#!/usr/local/sbin/click-install -uct12
define ($DST_MAC 00:15:17:51:d3:d4);
define ($DST_MAC1 00:25:17:51:d3:d4);
define ($HEADROOM_SIZE 34);
define ($BURST 64);
define ($SRC_PORT 5012);
define ($DST_PORT 5002);
define ($PKT_COUNT 5000000000);
define ($SRC_MAC 00:1a:92:9b:4a:77);
define ($SRC_MAC1 00:1a:92:9b:4a:71);
define ($COUNT 2000000000);
define ($PAYLOAD_SIZE 30);
//define ($PAYLOAD_SIZE 222);
//define ($PAYLOAD_SIZE 228);
//define ($PAYLOAD_SIZE 484);

define($IP_RT_SIZE 351611);
define($IP_RANDOMIZE_MAX_CYCLE $IP_RT_SIZE);

AddressInfo(
    RANDOM_IP 0.0.0.0,
    UNROUTABLE_IP 192.168.10.10,
    //RANDOM_IP6 0::0,
    //UNROUTABLE_IP6 1234::5678,
);

Idle()-> route::DirectIPLookup()
route[0]->Discard
route[1]->Discard
route[2]->Discard
route[3]->Discard
route[4]->Discard
Script(write route.load /home/dongsuh/xia-core/click-2.0/conf/xia/ip_routes_4_bal.txt);

elementclass gen_sub {
    $dev, $dev2, $queue, $cpu, $offset|

    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 120, BURST 120)
    -> IPEncap(9, UNROUTABLE_IP, RANDOM_IP)	     // Alternatively you can change UNROUTABLE_IP to RANDOM_IP in the source 
    -> DynamicIPEncap(0x99, $dev, $dev2, COUNT 120)  // This is to evenly distribute packet for RSS. (Alternative: see above comment)
    -> EtherEncap(0x0800, $SRC_MAC1 , $DST_MAC1)
    -> MarkIPHeader(34)
    -> clone ::Clone($COUNT, SHARED_SKBS false)
    -> IPRandomize(MAX_CYCLE $IP_RANDOMIZE_MAX_CYCLE, ROUTETABLENAME route, OFFSET 0)
    -> tod :: MQToDevice($dev, QUEUE $queue, BURST $BURST) 

    //StaticThreadSched(clone $cpu);
    StaticThreadSched(tod $cpu);

    StaticThreadSched(gen1 $cpu);
    Script(write gen1.active true);
}

elementclass gen {
    $dev, $dev2 |
    gen_sub($dev, $dev2, 0, 0, 0);
    gen_sub($dev, $dev2, 1, 1, 1000);
    gen_sub($dev, $dev2, 2, 2, 2000);
    gen_sub($dev, $dev2, 3, 3, 3000);
    gen_sub($dev, $dev2, 4, 4, 4000);
    gen_sub($dev, $dev2, 5, 5, 5000);
    gen_sub($dev, $dev2, 6, 6, 6000);
    gen_sub($dev, $dev2, 7, 7, 7000);
    gen_sub($dev, $dev2, 8, 8, 8000);
    gen_sub($dev, $dev2, 9, 9, 9000);
    gen_sub($dev, $dev2, 10, 10, 10000);
    gen_sub($dev, $dev2, 11, 11, 11000);
}

//gen(eth2)
//gen(eth3)
//gen(eth4)
//gen(eth5)
gen(xge0, xge2)
gen(xge1, xge3)
gen(xge2, xge0)
gen(xge3, xge1)

