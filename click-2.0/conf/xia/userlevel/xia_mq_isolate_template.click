define ($PAYLOAD_SIZE 130);
define ($COUNT 1000000000)

define ($DST_MAC 00:15:17:51:d3:d4);
define ($DST_MAC1 00:25:17:51:d3:d4);
define ($HEADROOM_SIZE 256);
define ($BURST 32);
define ($SRC_MAC 00:1a:92:9b:4a:77);
define ($SRC_MAC1 00:1a:92:9b:4a:71);

define ($ETH_P_IPV6 0x86DD)
define ($ETH_P_IP 0x0800)

//require(library ../xia_mq_template.click);
require(library ../xia_address.click);

elementclass cid_pkt {
    $hid_from, $ad, $cid_intent, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
    -> Script(TYPE PACKET, write gen1.active false) 
    -> XIAEncap(
           DST RE  $ad $cid_intent ,
           SRC RE  $hid_from, DYNAMIC false) -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass sid_pkt {
    $sid_from, $ad, $sid_to, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
    -> Script(TYPE PACKET, write gen1.active false) 
    -> XIAEncap(
           DST RE  $ad $sid_to, 
           SRC RE  $sid_from, DYNAMIC false) -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass clone {
    $dev, $src, $dst, $queue, $cpu |

    input 
    -> XIAPrint($dev) 
    -> IPEncap(0x99, $src, $dst)
    -> IPPrint($dev)
    -> EtherEncap($ETH_P_IP, $SRC_MAC1 , $DST_MAC1)
    -> clone1 ::Clone($COUNT, SHARED_SKBS true)
    -> td1 :: MQToDevice($dev, QUEUE $queue, BURST $BURST);
    StaticThreadSched(td1 $cpu);
}

elementclass gen_mix1 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID0, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    sid_pkt(SID0, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    sid_pkt(SID0, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    sid_pkt(SID0, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    sid_pkt(SID0, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    sid_pkt(SID0, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    sid_pkt(SID0, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    sid_pkt(SID0, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    sid_pkt(SID0, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID0, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID0, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID0, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    //cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    //cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    //cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    //cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    //cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    //cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    //cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    //cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix2 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    //cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    //cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    //cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    //cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    //cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    //cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    //cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}


elementclass gen_mix3 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    //cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    //cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    //cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    //cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    //cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    //cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix4 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    //sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    //cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    //cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    //cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    //cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    //cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix5 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    //sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    //sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    //cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    //cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    //cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    //cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix6 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    //sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    //sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    //sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    //cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    //cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    //cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix7 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    //sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    //sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    //sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    //sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    //cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    //cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix8 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    //sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    //sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    //sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    //sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    //sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    //cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix9 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    //sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    //sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    //sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    //sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    //sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    //sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    //cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix10 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    //sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    //sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    //sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    //sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    //sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    //sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    //sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    //cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}

elementclass gen_mix11 {
    $dev, $ad0, $ad1, $ad2, $ad3, $ad4, $ad5, $ad6, $ad7, $ad8, $ad9, $ad10, $ad11 |
    //sid_pkt(SID5, $ad0, SID1, 0) -> clone($dev, 10.0.0.1, 10.0.2.15, 0, 0);
    //sid_pkt(SID5, $ad1, SID1, 1) -> clone($dev, 63.0.0.1, 63.0.0.7, 1, 1);
    //sid_pkt(SID5, $ad2, SID1, 2) -> clone($dev, 63.0.0.1, 63.0.0.1, 2, 2);
    //sid_pkt(SID5, $ad3, SID1, 3) -> clone($dev, 10.0.0.1, 10.0.2.9, 3, 3);
    //sid_pkt(SID5, $ad4, SID1, 4) -> clone($dev, 10.0.0.1, 10.0.2.10, 4, 4);
    //sid_pkt(SID5, $ad5, SID1, 5) -> clone($dev, 63.0.0.1, 63.0.0.2, 5, 5);
    //sid_pkt(SID5, $ad6, SID1, 6) -> clone($dev, 63.0.0.1, 63.0.0.4, 6, 6);

    //sid_pkt(SID5, $ad7, SID1, 7) -> clone($dev, 10.0.0.1, 10.0.2.6, 7, 7);
    //sid_pkt(SID5, $ad8, SID1, 8) -> clone($dev, 10.0.0.1, 10.0.2.5, 8, 8);
    //sid_pkt(SID5, $ad9, SID1, 9) -> clone($dev, 10.0.0.1, 10.0.2.16, 9, 9);
    //sid_pkt(SID5, $ad10, SID1, 10) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    sid_pkt(SID5, $ad11, SID1, 11) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
    
    cid_pkt(HID5, $ad0, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.15, 0, 0);
    cid_pkt(HID5, $ad1, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.7, 1, 1);
    cid_pkt(HID5, $ad2, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.1, 2, 2);
    cid_pkt(HID5, $ad3, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.9, 3, 3);
    cid_pkt(HID5, $ad4, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.10, 4, 4);
    cid_pkt(HID5, $ad5, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.2, 5, 5);
    cid_pkt(HID5, $ad6, CID0, 0) -> clone($dev,  63.0.0.1, 63.0.0.4, 6, 6);

    cid_pkt(HID5, $ad7, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.6, 7, 7);
    cid_pkt(HID5, $ad8, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.5, 8, 8);
    cid_pkt(HID5, $ad9, CID0, 0) -> clone($dev,  10.0.0.1, 10.0.2.16, 9, 9);
    cid_pkt(HID5, $ad10, CID0, 0) -> clone($dev, 10.0.0.1, 10.0.2.8, 10, 10);
    //cid_pkt(HID5, $ad11, CID0, 0) -> clone($dev, 63.0.0.1, 63.0.0.0, 11, 11);
}
