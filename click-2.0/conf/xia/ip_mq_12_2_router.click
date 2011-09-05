#!/usr/local/sbin/click-install -uct12

//eth2 10.0.0.1 eth2 
//eth3 10.0.1.1 eth3 
//eth4 10.0.2.1 eth4 
//eth5 10.0.3.1 eth5 

elementclass router {
    $eth_idx, $from_eth, $from_queue0, $from_queue1, $to_queue, $ip, $cpu |

    pd0 :: MQPollDevice($from_eth, QUEUE $from_queue0, BURST 32, PROMISC true);
    pd1 :: MQPollDevice($from_eth, QUEUE $from_queue1, BURST 32, PROMISC true);
    td_eth2 :: MQPushToDevice(eth2, QUEUE $to_queue, BURST 32);
    td_eth3 :: MQPushToDevice(eth3, QUEUE $to_queue, BURST 32);

    ip :: Strip(14)
        -> CheckIPHeader(INTERFACES 10.0.0.1/255.255.255.0 10.0.1.1/255.255.255.0)
        -> rt :: SortedIPLookup(
            10.0.0.0/255.255.255.0 0,
            10.0.1.0/255.255.255.0 1
        );

    c :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
    pd0 -> c;
    pd1 -> c;
    c[0] -> Discard;
    c[1] -> Discard; //ARP response
    c[2] -> Paint($eth_idx) -> ip;
    c[3] -> Discard;

    // Forwarding path for eth2
    rt[0] -> DropBroadcasts
        -> cp0 :: PaintTee(1)
        -> gio0 :: IPGWOptions(10.0.0.1)
        -> FixIPSrc(10.0.0.1)
        -> dt0 :: DecIPTTL
        -> fr0 :: IPFragmenter(1500)
        //-> [0]arpq0;
        -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2) 
        -> td_eth2;
    dt0[1] -> ICMPError(10.0.0.1, timeexceeded) -> rt;
    fr0[1] -> ICMPError(10.0.0.1, unreachable, needfrag) -> rt;
    gio0[1] -> ICMPError(10.0.0.1, parameterproblem) -> rt;
    cp0[1] -> ICMPError(10.0.0.1, redirect, host) -> rt;

    // Forwarding path for eth3
    rt[1] -> DropBroadcasts
        -> cp1 :: PaintTee(2)
        -> gio1 :: IPGWOptions(10.0.1.1)
        -> FixIPSrc(10.0.1.1)
        -> dt1 :: DecIPTTL
        -> fr1 :: IPFragmenter(1500)
        //-> [0]arpq1;
        -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2) 
        -> td_eth3;
    dt1[1] -> ICMPError(10.0.1.1, timeexceeded) -> rt;
    fr1[1] -> ICMPError(10.0.1.1, unreachable, needfrag) -> rt;
    gio1[1] -> ICMPError(10.0.1.1, parameterproblem) -> rt;
    cp1[1] -> ICMPError(10.0.1.1, redirect, host) -> rt;

    StaticThreadSched(pd0 $cpu, pd1 $cpu);
}

router(1, eth2, 0, 1, 0, 10.0.0.1, 0);
router(1, eth2, 2, 3, 1, 10.0.0.1, 2);
router(1, eth2, 4, 5, 2, 10.0.0.1, 4);
router(1, eth2, 6, 7, 3, 10.0.0.1, 6);
router(1, eth2, 8, 9, 4, 10.0.0.1, 8);
router(1, eth2, 10, 11, 5, 10.0.0.1, 10);

router(2, eth3, 0, 1, 6, 10.0.1.1, 1);
router(2, eth3, 2, 3, 7, 10.0.1.1, 3);
router(2, eth3, 4, 5, 8, 10.0.1.1, 5);
router(2, eth3, 6, 7, 9, 10.0.1.1, 7);
router(2, eth3, 8, 9, 10, 10.0.1.1, 9);
router(2, eth3, 10, 11, 11, 10.0.1.1, 11);

