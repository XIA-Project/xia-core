#!/usr/local/sbin/click-install -uct12

//eth2 10.0.0.1 eth2 
//eth3 10.0.1.1 eth3 
//eth4 10.0.2.1 eth4 
//eth5 10.0.3.1 eth5 

elementclass router {
    $eth_idx, $from_eth, $from_queue0, $from_queue1, $from_queue2, $from_queue3, $to_queue, $ip, $cpu |

    pd0 :: MQPollDevice($from_eth, QUEUE $from_queue0, BURST 64, PROMISC true);
    pd1 :: MQPollDevice($from_eth, QUEUE $from_queue1, BURST 64, PROMISC true);
    pd2 :: MQPollDevice($from_eth, QUEUE $from_queue2, BURST 64, PROMISC true);
    pd3 :: MQPollDevice($from_eth, QUEUE $from_queue3, BURST 64, PROMISC true);
    td_eth2 :: MQPushToDevice(eth2, QUEUE $to_queue, BURST 64);
    td_eth3 :: MQPushToDevice(eth3, QUEUE $to_queue, BURST 64);
    td_eth4 :: MQPushToDevice(eth4, QUEUE $to_queue, BURST 64);
    td_eth5 :: MQPushToDevice(eth5, QUEUE $to_queue, BURST 64);

    ip :: Strip(14)
        -> CheckIPHeader(INTERFACES 10.0.0.1/255.255.255.0 10.0.1.1/255.255.255.0 10.0.2.1/255.255.255.0 10.0.3.1/255.255.255.0)
        -> rt :: RadixIPLookup(
            10.0.0.1/32 0,
            10.0.0.255/32 0,
            10.0.0.0/32 0,
            10.0.1.1/32 0,
            10.0.1.255/32 0,
            10.0.1.0/32 0,
            10.0.2.1/32 0,
            10.0.2.255/32 0,
            10.0.2.0/32 0,
            10.0.3.1/32 0,
            10.0.3.255/32 0,
            10.0.3.0/32 0,
            10.0.0.0/255.255.255.0 1,
            10.0.1.0/255.255.255.0 2,
            10.0.2.0/255.255.255.0 3,
            10.0.3.0/255.255.255.0 4,
            255.255.255.255/32 0.0.0.0 0,
            0.0.0.0/32 0,
            131.179.80.139/255.255.255.255 131.179.80.139 4);

    c :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
    pd0 -> c;
    pd1 -> c;
    pd2 -> c;
    pd3 -> c;
    //c[0] -> ar :: ARPResponder($ip $from_eth) -> td;
    c[0] -> Discard;
    c[1] -> Discard; //ARP response
    c[2] -> Paint($eth_idx) -> ip;
    c[3] -> Discard;

    // Local delivery
    toh :: Discard;
    rt[0] ->  toh;

    // Forwarding path for eth2
    rt[1] -> DropBroadcasts
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
    rt[2] -> DropBroadcasts
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

    // Forwarding path for eth4
    rt[3] -> DropBroadcasts
        -> cp2 :: PaintTee(3)
        -> gio2 :: IPGWOptions(10.0.2.1)
        -> FixIPSrc(10.0.2.1)
        -> dt2 :: DecIPTTL
        -> fr2 :: IPFragmenter(1500)
        //-> [0]arpq2;
        -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2) 
        -> td_eth4;
    dt2[1] -> ICMPError(10.0.2.1, timeexceeded) -> rt;
    fr2[1] -> ICMPError(10.0.2.1, unreachable, needfrag) -> rt;
    gio2[1] -> ICMPError(10.0.2.1, parameterproblem) -> rt;
    cp2[1] -> ICMPError(10.0.2.1, redirect, host) -> rt;

    // Forwarding path for eth5
    rt[4] -> DropBroadcasts
        -> cp3 :: PaintTee(4)
        -> gio3 :: IPGWOptions(10.0.3.1)
        -> FixIPSrc(10.0.3.1)
        -> dt3 :: DecIPTTL
        -> fr3 :: IPFragmenter(1500)
        //-> [0]arpq3;
        -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2) 
        -> td_eth5;
    dt3[1] -> ICMPError(10.0.3.1, timeexceeded) -> rt;
    fr3[1] -> ICMPError(10.0.3.1, unreachable, needfrag) -> rt;
    gio3[1] -> ICMPError(10.0.3.1, parameterproblem) -> rt;
    cp3[1] -> ICMPError(10.0.3.1, redirect, host) -> rt;

    StaticThreadSched(pd0 $cpu, pd1 $cpu, pd2 $cpu, pd3 $cpu);
}

router(1, eth2, 0, 1, 2, 3, 0, 10.0.0.1, 0);
router(1, eth2, 4, 5, 6, 7, 1, 10.0.0.1, 1);
router(1, eth2, 8, 9, 10, 11, 2, 10.0.0.1, 2);

router(2, eth3, 0, 1, 2, 3, 3, 10.0.1.1, 3);
router(2, eth3, 4, 5, 6, 7, 4, 10.0.1.1, 4);
router(2, eth3, 8, 9, 10, 11, 5, 10.0.1.1, 5);

router(3, eth4, 0, 1, 2, 3, 6, 10.0.2.1, 6);
router(3, eth4, 4, 5, 6, 7, 7, 10.0.2.1, 7);
router(3, eth4, 8, 9, 10, 11, 8, 10.0.2.1, 8);

router(4, eth5, 0, 1, 2, 3, 9, 10.0.3.1, 9);
router(4, eth5, 4, 5, 6, 7, 10, 10.0.3.1, 10);
router(4, eth5, 8, 9, 10, 11, 11, 10.0.3.1, 11);

