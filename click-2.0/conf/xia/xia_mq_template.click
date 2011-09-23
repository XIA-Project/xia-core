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

define ($ETH_P_IPV6 0x86DD)
define ($ETH_P_IP 0x0800)

elementclass gen_sub {
    $eth_from, $eth_to, $queue, $cpu |

    //pd1 :: MQPollDevice($eth_from, QUEUE $queue, PROMISC true) -> Discard;
    //StaticThreadSched(pd1 $cpu);

    input 
    //-> XIAPrint($eth_from) 
    -> DynamicIPEncap(0x99, $eth_from, $eth_to, COUNT 120)
    //-> IPEncap(0x99, $eth_from, $eth_to)
    -> EtherEncap($ETH_P_IP, $SRC_MAC1 , $DST_MAC1)
    //-> clone1 ::Clone($COUNT, SHARED_SKBS true, WAITUNTIL 240)
    -> td1 :: MQToDevice($eth_from, QUEUE $queue, BURST $BURST);
    StaticThreadSched(td1 $cpu);
}

elementclass nofb {
    $hid_from, $hid_to, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 240, BURST 20)
    -> XIAEncap(
           DST RE  $hid_to,
           SRC RE  $hid_from, DYNAMIC false) 
    -> MarkXIAHeader()
    -> XIAInsertHash()
    -> XIAPrint("XX" ,PAYLOAD HEX, LENGTH true, MAXLENGTH 256)
    -> Print(MAXLENGTH 256)
    -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass rgen_sub {
    $eth_from, $eth_to, $queue, $cpu | 

    input ->
    //-> MarkXIAHeader()
    -> XIARandomize(XID_TYPE AD, MAX_CYCLE $AD_RANDOMIZE_MAX_CYCLE)
    //-> Print(MAXLENGTH 256)
    -> td1 :: MQToDevice($eth_from, QUEUE $queue, BURST $BURST);

    //StaticThreadSched(td1 $cpu, clone $cpu);
    StaticThreadSched(td1 $cpu);
}

elementclass rnofb {
    $eth_from, $eth_to, $hid_from, $hid_to, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 120, BURST 120)
    -> XIAEncap(
           DST RE  $hid_to,
           SRC RE  $hid_from, DYNAMIC false) 
    -> DynamicIPEncap(0x99, $eth_from, $eth_to, COUNT 120)
    -> EtherEncap($ETH_P_IP, $SRC_MAC1 , $DST_MAC1)
    -> MarkXIAHeader(34)
    -> Clone($COUNT)
    -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass rgen_nofb_0 {
    $hid_from,  $eth_from, $eth_to |

    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 0) -> rgen_sub($eth_from,  $eth_to, 0, 0 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 1) -> rgen_sub($eth_from,  $eth_to, 1, 1 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 2) -> rgen_sub($eth_from,  $eth_to, 2, 2 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 3) -> rgen_sub($eth_from,  $eth_to, 3, 3 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 4) -> rgen_sub($eth_from,  $eth_to, 4, 4 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 5) -> rgen_sub($eth_from,  $eth_to, 5, 5 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 6) -> rgen_sub($eth_from,  $eth_to, 6, 6 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 7) -> rgen_sub($eth_from,  $eth_to, 7, 7 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 8) -> rgen_sub($eth_from,  $eth_to, 8, 8 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 9) -> rgen_sub($eth_from,  $eth_to, 9, 9 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 10) -> rgen_sub($eth_from, $eth_to, 10, 10 )
    rnofb($eth_from,  $eth_to, $hid_from, RANDOM_ID, 11) -> rgen_sub($eth_from, $eth_to, 11, 11 )
}

elementclass rgen_nofb_1 {
    $hid_from,  $eth_from, $eth_to |

    rnofb($hid_from, RANDOM_ID, 12) -> rgen_sub($eth_from, $eth_to, 0, 12) 
    rnofb($hid_from, RANDOM_ID, 13) -> rgen_sub($eth_from, $eth_to, 1, 13) 
    rnofb($hid_from, RANDOM_ID, 14) -> rgen_sub($eth_from, $eth_to, 2, 14) 
    rnofb($hid_from, RANDOM_ID, 15) -> rgen_sub($eth_from, $eth_to, 3, 15) 
    rnofb($hid_from, RANDOM_ID, 16) -> rgen_sub($eth_from, $eth_to, 4, 16) 
    rnofb($hid_from, RANDOM_ID, 17) -> rgen_sub($eth_from, $eth_to, 5, 17) 
    rnofb($hid_from, RANDOM_ID, 18) -> rgen_sub($eth_from, $eth_to, 6, 18) 
    rnofb($hid_from, RANDOM_ID, 19) -> rgen_sub($eth_from, $eth_to, 7, 19) 
    rnofb($hid_from, RANDOM_ID, 20) -> rgen_sub($eth_from, $eth_to, 8, 20) 
    rnofb($hid_from, RANDOM_ID, 21) -> rgen_sub($eth_from, $eth_to, 9, 21) 
    rnofb($hid_from, RANDOM_ID, 22) -> rgen_sub($eth_from,$eth_to, 10, 22)
    rnofb($hid_from, RANDOM_ID, 23) -> rgen_sub($eth_from,$eth_to, 11, 23)
}



elementclass gen_nofb_0 {
    $hid_from, $hid_to, $eth_from, $eth_to |

    nofb($hid_from, $hid_to, 0) -> gen_sub($eth_from,  $eth_to, 0, 0 )
    nofb($hid_from, $hid_to, 1) -> gen_sub($eth_from,  $eth_to, 1, 1 )
    nofb($hid_from, $hid_to, 2) -> gen_sub($eth_from,  $eth_to, 2, 2 )
    nofb($hid_from, $hid_to, 3) -> gen_sub($eth_from,  $eth_to, 3, 3 )
    nofb($hid_from, $hid_to, 4) -> gen_sub($eth_from,  $eth_to, 4, 4 )
    nofb($hid_from, $hid_to, 5) -> gen_sub($eth_from,  $eth_to, 5, 5 )
    nofb($hid_from, $hid_to, 6) -> gen_sub($eth_from,  $eth_to, 6, 6 )
    nofb($hid_from, $hid_to, 7) -> gen_sub($eth_from,  $eth_to, 7, 7 )
    nofb($hid_from, $hid_to, 8) -> gen_sub($eth_from,  $eth_to, 8, 8 )
    nofb($hid_from, $hid_to, 9) -> gen_sub($eth_from,  $eth_to, 9, 9 )
    nofb($hid_from, $hid_to, 10) -> gen_sub($eth_from, $eth_to, 10, 10 )
    nofb($hid_from, $hid_to, 11) -> gen_sub($eth_from, $eth_to, 11, 11 )
}

elementclass gen_nofb_1 {
    $hid_from, $hid_to, $eth_from, $eth_to |

    nofb($hid_from, $hid_to, 12) -> gen_sub($eth_from, $eth_to, 0, 12) 
    nofb($hid_from, $hid_to, 13) -> gen_sub($eth_from, $eth_to, 1, 13) 
    nofb($hid_from, $hid_to, 14) -> gen_sub($eth_from, $eth_to, 2, 14) 
    nofb($hid_from, $hid_to, 15) -> gen_sub($eth_from, $eth_to, 3, 15) 
    nofb($hid_from, $hid_to, 16) -> gen_sub($eth_from, $eth_to, 4, 16) 
    nofb($hid_from, $hid_to, 17) -> gen_sub($eth_from, $eth_to, 5, 17) 
    nofb($hid_from, $hid_to, 18) -> gen_sub($eth_from, $eth_to, 6, 18) 
    nofb($hid_from, $hid_to, 19) -> gen_sub($eth_from, $eth_to, 7, 19) 
    nofb($hid_from, $hid_to, 20) -> gen_sub($eth_from, $eth_to, 8, 20) 
    nofb($hid_from, $hid_to, 21) -> gen_sub($eth_from, $eth_to, 9, 21) 
    nofb($hid_from, $hid_to, 22) -> gen_sub($eth_from,$eth_to, 10, 22)
    nofb($hid_from, $hid_to, 23) -> gen_sub($eth_from,$eth_to, 11, 23)
}

elementclass rfb1 {
    $eth_from, $eth_to, $hid_from, $intent, $fb, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 120, BURST 120)
    -> XIAEncap(
           SRC RE  $hid_from,
           DST RE  ( $fb ) $intent, DYNAMIC false) 
    -> DynamicIPEncap(0x99, $eth_from, $eth_to, COUNT 120)
    //-> IPEncap(0x99, $eth_from, $eth_to)
    -> EtherEncap($ETH_P_IP, $SRC_MAC1 , $DST_MAC1)
    -> MarkXIAHeader(34)
    -> Clone($COUNT)
    -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass fb1 {
    $hid_from, $intent, $fb, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 240, BURST 20)
    -> XIAEncap(
           SRC RE  $hid_from,
           DST RE  ( $fb ) $intent, DYNAMIC false) 
    -> MarkXIAHeader()
    -> XIAInsertHash()
    -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass rgen_fb1_0 {
    $hid_from, $eth_from, $eth_to |

    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 0)  -> rgen_sub($eth_from, $eth_to, 0, 0 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 1)  -> rgen_sub($eth_from, $eth_to, 1, 1 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 2)  -> rgen_sub($eth_from, $eth_to, 2, 2 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 3)  -> rgen_sub($eth_from, $eth_to, 3, 3 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 4)  -> rgen_sub($eth_from, $eth_to, 4, 4 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 5)  -> rgen_sub($eth_from, $eth_to, 5, 5 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 6)  -> rgen_sub($eth_from, $eth_to, 6, 6 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 7)  -> rgen_sub($eth_from, $eth_to, 7, 7 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 8)  -> rgen_sub($eth_from, $eth_to, 8, 8 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 9)  -> rgen_sub($eth_from, $eth_to, 9, 9 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 10) -> rgen_sub($eth_from, $eth_to, 10, 10 )
    rfb1($eth_from, $eth_to, $hid_from, ARB_RANDOM_ID, RANDOM_ID, 11) -> rgen_sub($eth_from, $eth_to, 11, 11 )
}

elementclass rgen_fb1_1 {
    $hid_from, $eth_from, $eth_to |

    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 0) -> rgen_sub($eth_from, $eth_to, 0, 12) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 1) -> rgen_sub($eth_from, $eth_to, 1, 13) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 2) -> rgen_sub($eth_from, $eth_to, 2, 14) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 3) -> rgen_sub($eth_from, $eth_to, 3, 15) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 4) -> rgen_sub($eth_from, $eth_to, 4, 16) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 5) -> rgen_sub($eth_from, $eth_to, 5, 17) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 6) -> rgen_sub($eth_from, $eth_to, 6, 18) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 7) -> rgen_sub($eth_from, $eth_to, 7, 19) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 8) -> rgen_sub($eth_from, $eth_to, 8, 20) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 9) -> rgen_sub($eth_from, $eth_to, 9, 21) 
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 10) -> rgen_sub($eth_from,$eth_to, 10, 22)
    rfb1($hid_from, ARB_RANDOM_ID, RANDOM_ID, 11) -> rgen_sub($eth_from,$eth_to, 11, 23)
}


elementclass gen_fb1_0 {
    $hid_from, $intent, $fallback, $eth_from, $eth_to |

    fb1($hid_from, $intent, $fallback , 0) -> gen_sub($eth_from,$eth_to, 0, 0 )
    fb1($hid_from, $intent, $fallback , 1) -> gen_sub($eth_from,$eth_to, 1, 1 )
    fb1($hid_from, $intent, $fallback , 2) -> gen_sub($eth_from,$eth_to, 2, 2 )
    fb1($hid_from, $intent, $fallback , 3) -> gen_sub($eth_from,$eth_to, 3, 3 )
    fb1($hid_from, $intent, $fallback , 4) -> gen_sub($eth_from,$eth_to, 4, 4 )
    fb1($hid_from, $intent, $fallback , 5) -> gen_sub($eth_from,$eth_to, 5, 5 )
    fb1($hid_from, $intent, $fallback , 6) -> gen_sub($eth_from,$eth_to, 6, 6 )
    fb1($hid_from, $intent, $fallback , 7) -> gen_sub($eth_from,$eth_to, 7, 7 )
    fb1($hid_from, $intent, $fallback , 8) -> gen_sub($eth_from,$eth_to, 8, 8 )
    fb1($hid_from, $intent, $fallback , 9) -> gen_sub($eth_from,$eth_to, 9, 9 )
    fb1($hid_from, $intent, $fallback , 10) -> gen_sub($eth_from,$eth_to, 10, 10 )
    fb1($hid_from, $intent, $fallback , 11) -> gen_sub($eth_from,$eth_to, 11, 11 )
}

elementclass gen_fb1_1 {
    $hid_from, $intent, $fallback, $eth_from, $eth_to |

    fb1($hid_from, $intent, $fallback , 12) -> gen_sub($eth_from,$eth_to,  0, 12) 
    fb1($hid_from, $intent, $fallback , 13) -> gen_sub($eth_from,$eth_to,  1, 13) 
    fb1($hid_from, $intent, $fallback , 14) -> gen_sub($eth_from,$eth_to,  2, 14) 
    fb1($hid_from, $intent, $fallback , 15) -> gen_sub($eth_from,$eth_to,  3, 15) 
    fb1($hid_from, $intent, $fallback , 16) -> gen_sub($eth_from,$eth_to,  4, 16) 
    fb1($hid_from, $intent, $fallback , 17) -> gen_sub($eth_from,$eth_to,  5, 17) 
    fb1($hid_from, $intent, $fallback , 18) -> gen_sub($eth_from,$eth_to,  6, 18) 
    fb1($hid_from, $intent, $fallback , 19) -> gen_sub($eth_from,$eth_to,  7, 19) 
    fb1($hid_from, $intent, $fallback , 20) -> gen_sub($eth_from,$eth_to,  8, 20) 
    fb1($hid_from, $intent, $fallback , 21) -> gen_sub($eth_from,$eth_to,  9, 21) 
    fb1($hid_from, $intent, $fallback ,  22) -> gen_sub($eth_from,$eth_to, 10, 22)
    fb1($hid_from, $intent, $fallback ,  23) -> gen_sub($eth_from,$eth_to, 11, 23)
}

elementclass rfb2 {
    $eth_from, $eth_to, $hid_from, $intent, $fb1, $fb2, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 120, BURST 120)
    -> XIAEncap(
           SRC RE  $hid_from,
   	   DST DAG 			2 1 0 - // -1
	 		$fb2	  	2 -	// 0
		 	$fb1		2 -     // 1
		 	$intent			// 2
		, DYNAMIC false		)	
    -> DynamicIPEncap(0x99, $eth_from, $eth_to, COUNT 120)
    //-> IPEncap(0x99, $eth_from, $eth_to)
    -> EtherEncap($ETH_P_IP, $SRC_MAC1 , $DST_MAC1)
    -> MarkXIAHeader(34)
    -> Clone($COUNT)
    -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
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
		, DYNAMIC false		)	
    -> MarkXIAHeader()
    -> XIAInsertHash()
    -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass rgen_fb2_0 {
    $hid_from, $eth_from, $eth_to |

    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 0) 
//				-> MarkXIAHeader() -> XIAPrint()  
								  -> rgen_sub($eth_from,  $eth_to,  0, 0 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 1)  -> rgen_sub($eth_from,  $eth_to,  1, 1 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 2)  -> rgen_sub($eth_from,  $eth_to,  2, 2 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 3)  -> rgen_sub($eth_from,  $eth_to,  3, 3 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 4)  -> rgen_sub($eth_from,  $eth_to,  4, 4 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 5)  -> rgen_sub($eth_from,  $eth_to,  5, 5 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 6)  -> rgen_sub($eth_from,  $eth_to,  6, 6 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 7)  -> rgen_sub($eth_from,  $eth_to,  7, 7 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 8)  -> rgen_sub($eth_from,  $eth_to,  8, 8 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 9)  -> rgen_sub($eth_from,  $eth_to,  9, 9 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 10) -> rgen_sub($eth_from,  $eth_to, 10, 10 )
    rfb2($eth_from,  $eth_to, $hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 11) -> rgen_sub($eth_from,  $eth_to, 11, 11 )
}        
                  
elementclass rgen_fb2_1 {
    $hid_from, $eth_from, $eth_to |

    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 0) -> rgen_sub($eth_from,$eth_to,  0, 12) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 1) -> rgen_sub($eth_from,$eth_to,  1, 13) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 2) -> rgen_sub($eth_from,$eth_to,  2, 14) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 3) -> rgen_sub($eth_from,$eth_to,  3, 15) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 4) -> rgen_sub($eth_from,$eth_to,  4, 16) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 5) -> rgen_sub($eth_from,$eth_to,  5, 17) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 6) -> rgen_sub($eth_from,$eth_to,  6, 18) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 7) -> rgen_sub($eth_from,$eth_to,  7, 19) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 8) -> rgen_sub($eth_from,$eth_to,  8, 20) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 9) -> rgen_sub($eth_from,$eth_to,  9, 21) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 10) -> rgen_sub($eth_from,$eth_to, 10,22) 
    rfb2($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, RANDOM_ID , 11) -> rgen_sub($eth_from,$eth_to, 11,23)
}                                        
                                        

elementclass gen_fb2_0 {
    $hid_from, $intent, $fb1, $fb2, $eth_from, $eth_to |

    fb2($hid_from, $intent, $fb1, $fb2 , 0) -> gen_sub($eth_from,$eth_to,  0, 0 )
    fb2($hid_from, $intent, $fb1, $fb2 , 1) -> gen_sub($eth_from,$eth_to,  1, 1 )
    fb2($hid_from, $intent, $fb1, $fb2 , 2) -> gen_sub($eth_from,$eth_to,  2, 2 )
    fb2($hid_from, $intent, $fb1, $fb2 , 3) -> gen_sub($eth_from,$eth_to,  3, 3 )
    fb2($hid_from, $intent, $fb1, $fb2 , 4) -> gen_sub($eth_from,$eth_to,  4, 4 )
    fb2($hid_from, $intent, $fb1, $fb2 , 5) -> gen_sub($eth_from,$eth_to,  5, 5 )
    fb2($hid_from, $intent, $fb1, $fb2 , 6) -> gen_sub($eth_from,$eth_to,  6, 6 )
    fb2($hid_from, $intent, $fb1, $fb2 , 7) -> gen_sub($eth_from,$eth_to,  7, 7 )
    fb2($hid_from, $intent, $fb1, $fb2 , 8) -> gen_sub($eth_from,$eth_to,  8, 8 )
    fb2($hid_from, $intent, $fb1, $fb2 , 9) -> gen_sub($eth_from,$eth_to,  9, 9 )
    fb2($hid_from, $intent, $fb1, $fb2 , 10) -> gen_sub($eth_from,$eth_to, 10, 10 )
    fb2($hid_from, $intent, $fb1, $fb2 , 11) -> gen_sub($eth_from,$eth_to, 11, 11 )
}

elementclass gen_fb2_1 {
    $hid_from, $intent, $fb1, $fb2, $eth_from, $eth_to |

    fb2($hid_from, $intent, $fb1, $fb2  , 12) -> gen_sub($eth_from,$eth_to,  0, 12) 
    fb2($hid_from, $intent, $fb1, $fb2  , 13) -> gen_sub($eth_from,$eth_to,  1, 13) 
    fb2($hid_from, $intent, $fb1, $fb2  , 14) -> gen_sub($eth_from,$eth_to,  2, 14) 
    fb2($hid_from, $intent, $fb1, $fb2  , 15) -> gen_sub($eth_from,$eth_to,  3, 15) 
    fb2($hid_from, $intent, $fb1, $fb2  , 16) -> gen_sub($eth_from,$eth_to,  4, 16) 
    fb2($hid_from, $intent, $fb1, $fb2  , 17) -> gen_sub($eth_from,$eth_to,  5, 17) 
    fb2($hid_from, $intent, $fb1, $fb2  , 18) -> gen_sub($eth_from,$eth_to,  6, 18) 
    fb2($hid_from, $intent, $fb1, $fb2  , 19) -> gen_sub($eth_from,$eth_to,  7, 19) 
    fb2($hid_from, $intent, $fb1, $fb2  , 20) -> gen_sub($eth_from,$eth_to,  8, 20) 
    fb2($hid_from, $intent, $fb1, $fb2  , 21) -> gen_sub($eth_from,$eth_to,  9, 21) 
    fb2($hid_from, $intent, $fb1, $fb2  , 22) -> gen_sub($eth_from,$eth_to, 10, 22)
    fb2($hid_from, $intent, $fb1, $fb2  , 23) -> gen_sub($eth_from,$eth_to, 11, 23)
}

elementclass rfb3 {
    $eth_from, $eth_to, $hid_from, $intent, $fb1, $fb2, $fb3, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 120, BURST 120)
    -> XIAEncap(
           SRC RE  $hid_from,
   	   DST DAG 			3 2 1 0 // -1
	 		$fb3	  	3 -	// 0
	 		$fb2	  	3 -	// 1
		 	$fb1		3 -     // 2
		 	$intent			// 3
	 	, DYNAMIC false		)	
    -> DynamicIPEncap(0x99, $eth_from, $eth_to, COUNT 120)
    -> EtherEncap($ETH_P_IP, $SRC_MAC1 , $DST_MAC1)
    -> MarkXIAHeader(34)
    -> Clone($COUNT)
    -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass fb3 {
    $hid_from, $intent, $fb1, $fb2, $fb3, $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 24000000, BURST 256)
    -> XIAEncap(
           SRC RE  $hid_from,
           DST DAG 			3 2 1 0 // -1
         		$fb3	  	3 -	// 0
         		$fb2	  	3 -	// 1
        	 	$fb1		3 -     // 2
        	 	$intent			// 3
         	, DYNAMIC false		)	
    //-> MarkXIAHeader()
    //-> XIAInsertHash()
     -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass gen_fb3_0 {
    $hid_from, $intent, $fb1, $fb2, $fb3, $eth_from, $eth_to |

    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3 , 0) -> gen_sub($eth_from, $eth_to, 0, 0 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  1) -> gen_sub($eth_from, $eth_to, 1, 1 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  2) -> gen_sub($eth_from, $eth_to, 2, 2 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  3) -> gen_sub($eth_from, $eth_to, 3, 3 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  4) -> gen_sub($eth_from, $eth_to, 4, 4 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  5) -> gen_sub($eth_from, $eth_to, 5, 5 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  6) -> gen_sub($eth_from, $eth_to, 6, 6 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  7) -> gen_sub($eth_from, $eth_to, 7, 7 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  8) -> gen_sub($eth_from, $eth_to, 8, 8 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  9) -> gen_sub($eth_from, $eth_to, 9, 9 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  10) -> gen_sub($eth_from,$eth_to, 10, 10 )
    fb3($hid_from, $intent, $fb1, $fb2 ,$fb3,  11) -> gen_sub($eth_from,$eth_to, 11, 11 )
}

elementclass gen_fb3_1 {
    $hid_from, $intent, $fb1, $fb2, $fb3, $eth_from, $eth_to |

    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 12) -> gen_sub($eth_from, $eth_to, 0, 12) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 13) -> gen_sub($eth_from, $eth_to, 1, 13) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 14) -> gen_sub($eth_from, $eth_to, 2, 14) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 15) -> gen_sub($eth_from, $eth_to, 3, 15) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 16) -> gen_sub($eth_from, $eth_to, 4, 16) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 17) -> gen_sub($eth_from, $eth_to, 5, 17) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 18) -> gen_sub($eth_from, $eth_to, 6, 18) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 19) -> gen_sub($eth_from, $eth_to, 7, 19) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 20) -> gen_sub($eth_from, $eth_to, 8, 20) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 21) -> gen_sub($eth_from, $eth_to, 9, 21) 
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 22) -> gen_sub($eth_from, $eth_to, 10, 22)
    fb3($hid_from, $intent, $fb1, $fb2, $fb3, 23) -> gen_sub($eth_from, $eth_to, 11, 23)
}                                       

elementclass rgen_fb3_0 {
    $hid_from, $eth_from, $eth_to |

    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 0) 
//	-> MarkXIAHeader() -> XIAPrint()  
	->  rgen_sub($eth_from, $eth_to, 0, 0 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 1) ->  rgen_sub($eth_from, $eth_to, 1, 1 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 2) ->  rgen_sub($eth_from, $eth_to, 2, 2 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 3) ->  rgen_sub($eth_from, $eth_to, 3, 3 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 4) ->  rgen_sub($eth_from, $eth_to, 4, 4 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 5) ->  rgen_sub($eth_from, $eth_to, 5, 5 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 6) ->  rgen_sub($eth_from, $eth_to, 6, 6 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 7) ->  rgen_sub($eth_from, $eth_to, 7, 7 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 8) ->  rgen_sub($eth_from, $eth_to, 8, 8 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 9) ->  rgen_sub($eth_from, $eth_to, 9, 9 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 10) -> rgen_sub($eth_from, $eth_to, 10, 10 )
    rfb3($eth_from, $eth_to, $hid_from,  ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 11) -> rgen_sub($eth_from, $eth_to, 11, 11 )
}

elementclass rgen_fb3_1 {
    $hid_from, $eth_from, $eth_to |

    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 12) -> rgen_sub($eth_from, $eth_to, 0, 12) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 13) -> rgen_sub($eth_from, $eth_to, 1, 13) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 14) -> rgen_sub($eth_from, $eth_to, 2, 14) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 15) -> rgen_sub($eth_from, $eth_to, 3, 15) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 16) -> rgen_sub($eth_from, $eth_to, 4, 16) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 17) -> rgen_sub($eth_from, $eth_to, 5, 17) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 18) -> rgen_sub($eth_from, $eth_to, 6, 18) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 19) -> rgen_sub($eth_from, $eth_to, 7, 19) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 20) -> rgen_sub($eth_from, $eth_to, 8, 20) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 21) -> rgen_sub($eth_from, $eth_to, 9, 21) 
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 22) -> rgen_sub($eth_from, $eth_to, 10, 22)
    rfb3($hid_from, ARB_RANDOM_ID, ARB_RANDOM_ID, ARB_RANDOM_ID , RANDOM_ID, 23) -> rgen_sub($eth_from, $eth_to, 11, 23)
}
                                       
elementclass via {
    $eth_from, $eth_to, $hid_from, $viapoint, $intent,  $cpu |
    gen1:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE, LIMIT 120, BURST 120)
    -> XIAEncap(
           SRC RE  $hid_from,
   	   DST DAG  0 -  		// -1
	        $viapoint 1 - 		//  
	        $intent -
	   
	, DYNAMIC false)	
    -> DynamicIPEncap(0x99, $eth_from, $eth_to, COUNT 120)
    -> EtherEncap($ETH_P_IP, $SRC_MAC1 , $DST_MAC1)
    -> MarkXIAHeader(34)
    -> Clone($COUNT)
    -> output
    Script(write gen1.active true);
    StaticThreadSched(gen1 $cpu);
}

elementclass rgen_via_0 {
    $hid_from, $eth_from, $eth_to |

    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 0) 
//			-> MarkXIAHeader() -> XIAPrint()  
					   ->  rgen_sub($eth_from, $eth_to, 0, 0 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 1) ->  rgen_sub($eth_from, $eth_to, 1, 1 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 2) ->  rgen_sub($eth_from, $eth_to, 2, 2 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 3) ->  rgen_sub($eth_from, $eth_to, 3, 3 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 4) ->  rgen_sub($eth_from, $eth_to, 4, 4 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 5) ->  rgen_sub($eth_from, $eth_to, 5, 5 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 6) ->  rgen_sub($eth_from, $eth_to, 6, 6 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 7) ->  rgen_sub($eth_from, $eth_to, 7, 7 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 8) ->  rgen_sub($eth_from, $eth_to, 8, 8 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 9) ->  rgen_sub($eth_from, $eth_to, 9, 9 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 10) -> rgen_sub($eth_from, $eth_to, 10, 10 )
    via($eth_from, $eth_to, $hid_from,  SELFAD , RANDOM_ID, 11) -> rgen_sub($eth_from, $eth_to, 11, 11 )
}
