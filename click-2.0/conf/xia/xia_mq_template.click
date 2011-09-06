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

define ($ETH_P_IPV6 0x86DD)
define ($ETH_P_IP 0x0800)

elementclass gen_sub {
    $eth_from, $queue, $cpu |

    pd1 :: MQPollDevice($eth_from, QUEUE $queue, PROMISC true) -> Discard;
    StaticThreadSched(pd1 $cpu);

    input -> XIAPrint($eth_from) ->Print(MAXLENGTH 64)
    -> EtherEncap($ETH_P_IPV6, $SRC_MAC1 , $DST_MAC1)
    -> clone1 ::Clone($COUNT, SHARED_SKBS true)
    -> td1 :: MQToDevice($eth_from, QUEUE $queue, BURST $BURST);
    StaticThreadSched(td1 $cpu);
}

elementclass nofb {
    $hid_from, $hid_to, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 240)
    -> XIAEncap(
           DST RE  $hid_to,
           SRC RE  $hid_from, DYNAMIC true) -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass gen_nofb_0 {
    $hid_from, $hid_to, $eth_from |

    nofb($hid_from, $hid_to, 0) -> gen_sub($eth_from, 0, 0 )
    nofb($hid_from, $hid_to, 1) -> gen_sub($eth_from, 1, 1 )
    nofb($hid_from, $hid_to, 2) -> gen_sub($eth_from, 2, 2 )
    nofb($hid_from, $hid_to, 3) -> gen_sub($eth_from, 3, 3 )
    nofb($hid_from, $hid_to, 4) -> gen_sub($eth_from, 4, 0 )
    nofb($hid_from, $hid_to, 5) -> gen_sub($eth_from, 5, 5 )
    nofb($hid_from, $hid_to, 6) -> gen_sub($eth_from, 6, 6 )
    nofb($hid_from, $hid_to, 7) -> gen_sub($eth_from, 7, 7 )
    nofb($hid_from, $hid_to, 8) -> gen_sub($eth_from, 8, 8 )
    nofb($hid_from, $hid_to, 9) -> gen_sub($eth_from, 9, 9 )
    nofb($hid_from, $hid_to, 10) -> gen_sub($eth_from, 10, 10 )
    nofb($hid_from, $hid_to, 11) -> gen_sub($eth_from, 11, 11 )
}

elementclass gen_nofb_1 {
    $hid_from, $hid_to, $eth_from |

    nofb($hid_from, $hid_to, 12) -> gen_sub($eth_from, 0, 12) 
    nofb($hid_from, $hid_to, 13) -> gen_sub($eth_from, 1, 13) 
    nofb($hid_from, $hid_to, 14) -> gen_sub($eth_from, 2, 14) 
    nofb($hid_from, $hid_to, 15) -> gen_sub($eth_from, 3, 15) 
    nofb($hid_from, $hid_to, 16) -> gen_sub($eth_from, 4, 16) 
    nofb($hid_from, $hid_to, 17) -> gen_sub($eth_from, 5, 17) 
    nofb($hid_from, $hid_to, 18) -> gen_sub($eth_from, 6, 18) 
    nofb($hid_from, $hid_to, 19) -> gen_sub($eth_from, 7, 19) 
    nofb($hid_from, $hid_to, 20) -> gen_sub($eth_from, 8, 20) 
    nofb($hid_from, $hid_to, 21) -> gen_sub($eth_from, 9, 21) 
    nofb($hid_from, $hid_to,  22) -> gen_sub($eth_from, 10, 22)
    nofb($hid_from, $hid_to,  23) -> gen_sub($eth_from, 11, 23)
}


elementclass fb1 {
    $hid_from, $intent, $fb, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 240)
    -> XIAEncap(
           SRC RE  $hid_from,
           DST RE  ( $fb ) $intent, DYNAMIC true) -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass gen_fb1_0 {
    $hid_from, $intent, $fallback, $eth_from |

    fb1($hid_from, $intent, $fallback , 0) -> gen_sub($eth_from, 0, 0 )
    fb1($hid_from, $intent, $fallback , 1) -> gen_sub($eth_from, 1, 1 )
    fb1($hid_from, $intent, $fallback , 2) -> gen_sub($eth_from, 2, 2 )
    fb1($hid_from, $intent, $fallback , 3) -> gen_sub($eth_from, 3, 3 )
    fb1($hid_from, $intent, $fallback , 4) -> gen_sub($eth_from, 4, 0 )
    fb1($hid_from, $intent, $fallback , 5) -> gen_sub($eth_from, 5, 5 )
    fb1($hid_from, $intent, $fallback , 6) -> gen_sub($eth_from, 6, 6 )
    fb1($hid_from, $intent, $fallback , 7) -> gen_sub($eth_from, 7, 7 )
    fb1($hid_from, $intent, $fallback , 8) -> gen_sub($eth_from, 8, 8 )
    fb1($hid_from, $intent, $fallback , 9) -> gen_sub($eth_from, 9, 9 )
    fb1($hid_from, $intent, $fallback , 10) -> gen_sub($eth_from, 10, 10 )
    fb1($hid_from, $intent, $fallback , 11) -> gen_sub($eth_from, 11, 11 )
}

elementclass gen_fb1_1 {
    $hid_from, $intent, $fallback, $eth_from |

    fb1($hid_from, $intent, $fallback , 12) -> gen_sub($eth_from, 0, 12) 
    fb1($hid_from, $intent, $fallback , 13) -> gen_sub($eth_from, 1, 13) 
    fb1($hid_from, $intent, $fallback , 14) -> gen_sub($eth_from, 2, 14) 
    fb1($hid_from, $intent, $fallback , 15) -> gen_sub($eth_from, 3, 15) 
    fb1($hid_from, $intent, $fallback , 16) -> gen_sub($eth_from, 4, 16) 
    fb1($hid_from, $intent, $fallback , 17) -> gen_sub($eth_from, 5, 17) 
    fb1($hid_from, $intent, $fallback , 18) -> gen_sub($eth_from, 6, 18) 
    fb1($hid_from, $intent, $fallback , 19) -> gen_sub($eth_from, 7, 19) 
    fb1($hid_from, $intent, $fallback , 20) -> gen_sub($eth_from, 8, 20) 
    fb1($hid_from, $intent, $fallback , 21) -> gen_sub($eth_from, 9, 21) 
    fb1($hid_from, $intent, $fallback ,  22) -> gen_sub($eth_from, 10, 22)
    fb1($hid_from, $intent, $fallback ,  23) -> gen_sub($eth_from, 11, 23)
}

elementclass fb2 {
    $hid_from, $intent, $fb1, $fb2, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 240)
    -> XIAEncap(
           SRC RE  $hid_from,
   	   DST DAG 			2 1 0 - // -1
	 		$fb2	  	2 -	// 0
		 	$fb1		2 -     // 1
		 	$intent			// 2
		, DYNAMIC true		)	
     -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass gen_fb2_0 {
    $hid_from, $intent, $fb1, $fb2, $eth_from |

    fb2($hid_from, $intent, $fb1, $fb2 , 0) -> gen_sub($eth_from, 0, 0 )
    fb2($hid_from, $intent, $fb1, $fb2 , 1) -> gen_sub($eth_from, 1, 1 )
    fb2($hid_from, $intent, $fb1, $fb2 , 2) -> gen_sub($eth_from, 2, 2 )
    fb2($hid_from, $intent, $fb1, $fb2 , 3) -> gen_sub($eth_from, 3, 3 )
    fb2($hid_from, $intent, $fb1, $fb2 , 4) -> gen_sub($eth_from, 4, 0 )
    fb2($hid_from, $intent, $fb1, $fb2 , 5) -> gen_sub($eth_from, 5, 5 )
    fb2($hid_from, $intent, $fb1, $fb2 , 6) -> gen_sub($eth_from, 6, 6 )
    fb2($hid_from, $intent, $fb1, $fb2 , 7) -> gen_sub($eth_from, 7, 7 )
    fb2($hid_from, $intent, $fb1, $fb2 , 8) -> gen_sub($eth_from, 8, 8 )
    fb2($hid_from, $intent, $fb1, $fb2 , 9) -> gen_sub($eth_from, 9, 9 )
    fb2($hid_from, $intent, $fb1, $fb2 , 10) -> gen_sub($eth_from, 10, 10 )
    fb2($hid_from, $intent, $fb1, $fb2 , 11) -> gen_sub($eth_from, 11, 11 )
}

elementclass gen_fb2_1 {
    $hid_from, $intent, $fb1, $fb2, $eth_from |

    fb2($hid_from, $intent, $fb1, $fb2  , 12) -> gen_sub($eth_from, 0, 12) 
    fb2($hid_from, $intent, $fb1, $fb2  , 13) -> gen_sub($eth_from, 1, 13) 
    fb2($hid_from, $intent, $fb1, $fb2  , 14) -> gen_sub($eth_from, 2, 14) 
    fb2($hid_from, $intent, $fb1, $fb2  , 15) -> gen_sub($eth_from, 3, 15) 
    fb2($hid_from, $intent, $fb1, $fb2  , 16) -> gen_sub($eth_from, 4, 16) 
    fb2($hid_from, $intent, $fb1, $fb2  , 17) -> gen_sub($eth_from, 5, 17) 
    fb2($hid_from, $intent, $fb1, $fb2  , 18) -> gen_sub($eth_from, 6, 18) 
    fb2($hid_from, $intent, $fb1, $fb2  , 19) -> gen_sub($eth_from, 7, 19) 
    fb2($hid_from, $intent, $fb1, $fb2  , 20) -> gen_sub($eth_from, 8, 20) 
    fb2($hid_from, $intent, $fb1, $fb2  , 21) -> gen_sub($eth_from, 9, 21) 
    fb2($hid_from, $intent, $fb1, $fb2  , 22) -> gen_sub($eth_from, 10, 22)
    fb2($hid_from, $intent, $fb1, $fb2  , 23) -> gen_sub($eth_from, 11, 23)
}

elementclass fb3 {
    $hid_from, $intent, $fb1, $fb2, $fb3, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 240)
    -> XIAEncap(
           SRC RE  $hid_from,
   	   DST DAG 			3 2 1 0 // -1
	 		$fb3	  	3 -	// 0
	 		$fb2	  	3 -	// 1
		 	$fb1		3 -     // 2
		 	$intent			// 3
		, DYNAMIC true		)	
     -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass gen_fb3_0 {
    $hid_from, $intent, $fb1, $fb2, $fb3, $eth_from |

    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3 , 0) -> gen_sub($eth_from, 0, 0 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  1) -> gen_sub($eth_from, 1, 1 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  2) -> gen_sub($eth_from, 2, 2 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  3) -> gen_sub($eth_from, 3, 3 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  4) -> gen_sub($eth_from, 4, 0 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  5) -> gen_sub($eth_from, 5, 5 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  6) -> gen_sub($eth_from, 6, 6 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  7) -> gen_sub($eth_from, 7, 7 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  8) -> gen_sub($eth_from, 8, 8 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  9) -> gen_sub($eth_from, 9, 9 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  10) -> gen_sub($eth_from, 10, 10 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  11) -> gen_sub($eth_from, 11, 11 )
}

elementclass gen_fb3_1 {
    $hid_from, $intent, $fb1, $fb2, $fb3, $eth_from |

    fb3($hid_from, $intent, $fb1, $fb2, $fb3 , 12) -> gen_sub($eth_from, 0, 12) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 13) -> gen_sub($eth_from, 1, 13) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 14) -> gen_sub($eth_from, 2, 14) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 15) -> gen_sub($eth_from, 3, 15) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 16) -> gen_sub($eth_from, 4, 16) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 17) -> gen_sub($eth_from, 5, 17) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 18) -> gen_sub($eth_from, 6, 18) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 19) -> gen_sub($eth_from, 7, 19) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 20) -> gen_sub($eth_from, 8, 20) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 21) -> gen_sub($eth_from, 9, 21) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 22) -> gen_sub($eth_from, 10, 22)
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 23) -> gen_sub($eth_from, 11, 23)
}                                       
