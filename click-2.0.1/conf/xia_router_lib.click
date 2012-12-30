require(library xia_constants.click);

elementclass XIAFromHost {
    $fake, $API_IP, $CLICK_IP, $ether_addr |

	// output[0]: control traffic from host (should go to [0]xtransport)
	// output[1]: data traffic from host (should go to [1]xtransport)

    // Create kernel TAP interface which responds to ARP
    fake0::FromHost($fake, $API_IP/24, CLICK_XTRANSPORT_ADDR $CLICK_IP, HEADROOM 256, MTU 65521) 
    -> fromhost_cl :: Classifier(12/0806, 12/0800) -> ARPResponder(0.0.0.0/0 $ether_addr) -> ToHost($fake);

    // Classifier to sort between control/normal
    fromhost_cl[1] -> StripToNetworkHeader()
    -> sorter::IPClassifier(dst udp port 5001 or 5002 or 5003 or 5004 or 5005 or 5006,
                            dst udp port 10000 or 10001 or 10002);

    // Control in (0); Socket side data in (1)
    sorter[0,1] => output;
}

elementclass XIAToHost {
    $fake, $ether_addr |
	// input: packets to send up (usually xtransport[1])	

    // socket side out
    input -> cIP::CheckIPHeader() -> EtherEncap(0x0800, $ether_addr, 11:11:11:11:11:11) -> ToHost($fake);	
    cIP[1] -> Print(bad, MAXLENGTH 100, CONTENTS ASCII) -> Discard();
}

elementclass GenericPostRouteProc {
    // output[0]: forward (decremented)
	// output[1]: hop limit reached (should send back a TTL expry packet)
    input -> XIADecHLIM[0,1] => output;
};

elementclass XIAPacketRoute {
    $local_addr, $num_ports |

    // $local_addr: the full address of the node (only used for debugging)

    // input: a packet to process
    // output[0]: forward (painted)
    // output[1]: arrived at destination node
    // output[2]: could not route at all (tried all paths)
	// output[3]: SID hack for DHCP functionality

    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, next IP, -);

    input -> consider_first_path :: XIASelectPath(first);

    // arrived at the final destination or reiterate paths with new last pointer
    check_dest :: XIACheckDest() -> XIAPaint($DESTINED_FOR_LOCALHOST) -> [1]output
	check_dest[1] -> consider_first_path;

	consider_next_path :: XIASelectPath(next);
    consider_first_path => c, [2]output;
	consider_next_path => c, [2]output;

    x :: XCMP($local_addr);
	x[1] -> Discard;
	GPRP :: GenericPostRouteProc -> [0]output;
	GPRP[1] -> x[0] -> consider_first_path;

	rt_AD, rt_HID, rt_SID, rt_CID, rt_IP :: XIAXIDRouteTable($local_addr, $num_ports);
    c => rt_AD, rt_HID, rt_SID, rt_CID, rt_IP, [2]output;
		
    rt_AD[0], rt_HID[0], rt_SID[0], rt_CID[0], rt_IP[0] -> GPRP;		
    rt_AD[1], rt_HID[1], 			rt_CID[1], rt_IP[1] -> XIANextHop -> check_dest;
			  			 rt_SID[1]			   			-> XIANextHop -> XIAPaint($DESTINED_FOR_LOCALHOST) -> [1]output;
    rt_AD[2], rt_HID[2], rt_SID[2], rt_CID[2], rt_IP[2] -> consider_next_path;
	rt_AD[3], rt_HID[3],            rt_CID[3], rt_IP[3] -> Discard;
			  			 rt_SID[3]                      -> [3]output;
	rt_AD[4], rt_HID[4], rt_SID[4], rt_CID[4], rt_IP[4] -> x; // possible xcmp redirect message
};


elementclass RouteEngine {
    $local_addr, $num_ports |

    // $local_addr: the full address of the node (only used for debugging)

    // input[0]: a packet arrived at the node from outside (i.e. routing with caching)
    // input[1]: a packet to send from a node (i.e. routing without caching)
    // output[0]: forward (painted)
    // output[1]: arrived at destination node; go to RPC
    // output[2]: arrived at destination node; go to cache

    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    proc :: XIAPacketRoute($local_addr, $num_ports);
    dstTypeClassifier :: XIAXIDTypeClassifier(dst CID, -);

    input[0] -> srcTypeClassifier;
    input[1] -> proc;

    srcTypeClassifier[0] -> cidFork :: Tee(2) -> [2]output;  // To cache (for content caching)
    cidFork[1] -> proc;                 // Main routing process

    srcTypeClassifier[1] -> proc;       // Main routing process

    proc[0] -> [0]output;               // Forward to other interface

    proc[1] -> dstTypeClassifier;
    dstTypeClassifier[1] -> [1]output;  // To RPC / Application

    dstTypeClassifier[0] -> [2]output;  // To cache (for serving content request)

    proc[2] -> x::XCMP($local_addr) -> proc; 
	x[1] -> Discard;
  
    // hack to use DHCP functionality
    proc[3] -> [3]output;
};

elementclass XIALineCard {
	$local_addr, $local_hid, $mac, $num |

	// input[0]: a packet arriving from the network
	// input[1]: a packet arriving from the higher stack (i.e. router or end host)
	// output[0]: send out to network
	// output[1]: send up to the higher stack (i.e. router or end host)

	// setup XARP module
    c :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/C0DE);  // XARP (query) or XARP (response) or XIP
    xarpq :: XARPQuerier($local_hid, $mac);
    xarpr :: XARPResponder($local_hid $mac);        

	// On receiving a packet from the host
	input[1] -> xarpq;
    
	// On receiving a packet from interface
	input[0] -> c;
    
    // Receiving an XIA packet
    c[2] -> Strip(14) -> MarkXIAHeader() -> XIAPaint($num) -> [1]output; // this should send out to [0]n; 

    toNet :: Queue(200) -> [0]output;
     
    // On receiving ARP response
    c[1] -> [1]xarpq -> toNet;
  
    // On receiving ARP query
    c[0] -> xarpr -> toNet;    

    // XAPR timeout to XCMP
	xarpq[1] -> x :: XCMP($local_addr) -> [1]output;
	x[1] -> Discard;
}

elementclass IPLineCard {
    $ip, $gw, $mac, $num |
	
	// input[0]: a packet arriving from the network
	// input[1]: a packet arriving from the higher stack (i.e. router or end host)
	// output[0]: send out to network
	// output[1]: send up to the higher stack (i.e. router or end host)

	// Set up ARP querier and responder
	// 			     ARP query        ARP response     IP      UDP   4ID port
    c :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800 23/11 36/03E9);
	arpq :: ARPQuerier($ip, $mac);
	arpr :: ARPResponder($ip/32 $mac);
    
	// On receiving a packet from the host
	// Sending an XIA-encapped-in-IP packet (via ARP if necessary)
	// If IP is in our subnet, do an ARP. Otherwise, send to the default gateway
	// dip sets the next IP annotation to the defualt gw so ARP knows what to query
	input[1] -> XIAIPEncap(SRC $ip) -> DirectIPLookup(0.0.0.0/0 $gw 0) -> arpq;

    // On receiving a packet from interface
    input[0] -> c; 

	// Receiving an XIA-encapped-in-IP packet; strip the ethernet, IP, and UDP headers, 
	// leaving bare XIP packet, then paint so we know which port it came in on
	c[2] -> Strip(14) -> MarkIPHeader -> StripIPHeader -> Strip(8) -> MarkXIAHeader
	-> Paint($num) -> [1]output; // this should be send out to [0]n;
   	
	toNet :: Queue(200) -> [0]output;

	// Receiving an ARP Response; return it to querier
	c[1] -> [1]arpq -> toNet;

	// Receiving an ARP Query; respond if it's interface's IP
	c[0] -> arpr -> toNet;
}

elementclass XIADualLineCard {
    $local_addr, $local_hid, $mac, $num, $ip , $gw, $ip_active |

 	// input[0]: a packet arriving from the network
	// input[1]: a packet arriving from the higher stack (i.e. router or end host)
	// output[0]: send out to network
	// output[1]: send up to the higher stack (i.e. router or end host)

	// 			                   XARP query       XARP response    XIP	
    input[0] -> c :: Classifier(12/9990 20/0001 or 12/9990 20/0002 or 12/C0DE, -);

	c[0] -> xlc :: XIALineCard($local_addr, $local_hid, $mac, $num) -> [0]output;
	c[1] -> iplc :: IPLineCard($ip, $gw, $mac, $num) -> [0]output;

	// Packet needs forwarding and has been painted w/ output port;
	// check if it's heading to an XIA network or an IP network
	input[1] -> dstTypeC :: XIAXIDTypeClassifier(next IP, -);

        // TODO: something here doesn't work. (push/pull weirdness?)
	dstTypeC[0], dstTypeC[1] 
                => suppress::Suppressor() 
                => [1]iplc[1], [1]xlc[1] 
                => [1]output, [1]output;

        // Tell surpressor which packets to drop based on whether or not IP is enabled
        // TODO: test this and then take out the reads
        Script(write suppress.active0 $ip_active,
               write suppress.active1 !$ip_active,
               print read suppess.active0,
               print read suppress.active1);
}

elementclass XIARoutingCore {
    $local_addr, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, $num_ports, $is_dual_stack |

    // input[0]: packet to route
	// output[0]: packet to be forwarded out a given port based on paint value

	n :: RouteEngine($local_addr, $num_ports);       
    
    xtransport::XTRANSPORT($local_addr, IP:$external_ip, $CLICK_IP, $API_IP, n/proc/rt_SID, IS_DUAL_STACK_ROUTER $is_dual_stack); 

	XIAFromHost($fake, $API_IP, $CLICK_IP, $ether_addr)[0,1] => xtransport;
	xtransport[1] -> XIAToHost($fake, $ether_addr);

    xtransport[0] -> Discard; // Port 0 is unused for now.
    
    cache :: XIACache($local_addr, n/proc/rt_CID, PACKET_SIZE 1400, MALICIOUS 0);

    Script(write n/proc/rt_HID.add $local_hid $DESTINED_FOR_LOCALHOST);  // self RHID as destination
    Script(write n/proc/rt_HID.add BHID $DESTINED_FOR_BROADCAST);  // outgoing broadcast packet
	Script(write n/proc/rt_HID.add - $FALLBACK);
    Script(write n/proc/rt_AD.add - $FALLBACK);     // no default route for AD; consider other path
    Script(write n/proc/rt_SID.add - $FALLBACK);     // no default route for SID; consider other path
    Script(write n/proc/rt_CID.add - $FALLBACK);     // no default route for CID; consider other path
	Script(write n/proc/rt_IP.add - $FALLBACK);		// no default route for IP; consider other path

    // quick fix
    n[3] -> Discard();
    Idle() -> [4]xtransport;
    
    // set up XCMP elements
    c :: Classifier(01/3D, -); // XCMP
    x :: XCMP($local_addr);	

    n[0] -> output;
	input -> [0]n;

    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    n[1] -> c[1] -> srcTypeClassifier[1] -> [2]xtransport[2] -> XIAPaint($DESTINED_FOR_LOCALHOST) -> [0]n;
    srcTypeClassifier[0] -> Discard;    // do not send CID responses directly to RPC;
    c[0] -> x[0] -> [0]n; // new (response) XCMP packets destined for some other machine
    
	x[1] -> rsw :: XIAPaintSwitch -> [2]xtransport; // XCMP packets destined for this machine
    rsw[1] -> XIAPaint($REDIRECT) -> [0]n; // XCMP redirect packet, so a route update will be done.

    n[2] -> [0]cache[0] -> XIAPaint($DESTINED_FOR_LOCALHOST) -> [1]n;
    // For get and put cid
    xtransport[3] -> [1]cache[1] -> [3]xtransport;
}

// 2-port router 
elementclass XIARouter2Port {
    $local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 
	$mac0, $mac1, |

    // $local_addr: the full address of the node
    // $external_ip: an ingress IP address for this XIA cloud (given to hosts via XHCP)  TODO: be able to handle more than one

    // input[0], input[1]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    
	xrc :: XIARoutingCore($local_addr, $local_hid, $external_ip, $fake, $CLICK_IP, 
		   				  $API_IP, $ether_addr, 2, 0);

    Script(write xrc/n/proc/rt_AD.add $local_ad $DESTINED_FOR_LOCALHOST);    // self AD as destination

	xlc0 :: XIALineCard($local_addr, $local_hid, $mac0, 0);
	xlc1 :: XIALineCard($local_addr, $local_hid, $mac1, 1);
    
	input => xlc0, xlc1 => output;
	xrc -> XIAPaintSwitch[0,1] => [1]xlc0[1], [1]xlc1[1]  -> [0]xrc;
};

// 2-port router instrumented with counters and prints
elementclass XIAInstrumentedRouter2Port {
    $local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, $mac0, $mac1 |

    wrapped :: XIARouter2Port($local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, $mac0, $mac1);

    print_in0 :: XIAPrint(">>> $local_hid (In Port 0)");
    print_in1 :: XIAPrint(">>> $local_hid (In Port 1)");
    print_out0 :: XIAPrint("<<< $local_hid (Out Port 0)");
    print_out1 :: XIAPrint("<<< $local_hid (Out Port 1)");

    count_final_out0 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out0 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);
    count_final_out1 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out1 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);

    input => print_in0, print_in1 
        => wrapped 
        => print_out0, print_out1 
        => count_final_out0, count_final_out1 
        => count_next_out0, count_next_out1 => output;
}

// 4-port router node 
elementclass XIARouter4Port {
    $local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 
	$mac0, $mac1, $mac2, $mac3 |

    // $local_addr: the full address of the node
    // $external_ip: an ingress IP address for this XIA cloud (given to hosts via XHCP)  TODO: be able to handle more than one
	// $malicious_cache: if set to 1, the content cache responds with bad content

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3
    
	xrc :: XIARoutingCore($local_addr, $local_hid, $external_ip, $fake, $CLICK_IP, 
		   				  $API_IP, $ether_addr, 4, 0);

    Script(write xrc/n/proc/rt_AD.add $local_ad $DESTINED_FOR_LOCALHOST);    // self AD as destination

	xlc0 :: XIALineCard($local_addr, $local_hid, $mac0, 0);
	xlc1 :: XIALineCard($local_addr, $local_hid, $mac1, 1);
	xlc2 :: XIALineCard($local_addr, $local_hid, $mac2, 2);
	xlc3 :: XIALineCard($local_addr, $local_hid, $mac3, 3);
    
	input => xlc0, xlc1, xlc2, xlc3 => output;
	xrc -> XIAPaintSwitch[0,1,2,3] => [1]xlc0[1], [1]xlc1[1], [1]xlc2[1], [1]xlc3[1] -> [0]xrc;
};

// 4-port router instrumented with counters and prints
elementclass XIAInstrumentedRouter4Port {
    $local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 
        $mac0, $mac1, $mac2, $mac3|

    wrapped :: XIARouter4Port($local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, $mac0, $mac1, $mac2, $mac3);

    print_in0 :: XIAPrint(">>> $local_hid (In Port 0)");
    print_in1 :: XIAPrint(">>> $local_hid (In Port 1)");
    print_in2 :: XIAPrint(">>> $local_hid (In Port 2)");
    print_in3 :: XIAPrint(">>> $local_hid (In Port 3)");
    print_out0 :: XIAPrint("<<< $local_hid (Out Port 0)");
    print_out1 :: XIAPrint("<<< $local_hid (Out Port 1)");
    print_out2 :: XIAPrint("<<< $local_hid (Out Port 2)");
    print_out3 :: XIAPrint("<<< $local_hid (Out Port 3)");

    count_final_out0 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out0 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);
    count_final_out1 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out1 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);
    count_final_out2 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out2 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);
    count_final_out3 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out3 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);

    input => print_in0, print_in1, print_in2, print_in3
        => wrapped 
        => print_out0, print_out1, print_out2, print_out3 
        => count_final_out0, count_final_out1, count_final_out2, count_final_out3
        => count_next_out0, count_next_out1, count_next_out2, count_next_out3 => output;
}

// 4-port router node with XRoute process running and IP support
elementclass XIADualRouter4Port {
    $local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 
	$ip_active0, $ip0, $mac0, $gw0,
	$ip_active1, $ip1, $mac1, $gw1,
	$ip_active2, $ip2, $mac2, $gw2, 
	$ip_active3, $ip3, $mac3, $gw3 |

	// NOTE: This router assumes that each port is connected to *either* an XIA network *or* an IP network.
	// If port 0 is connected to an IP network and is asked to send an XIA packet (e.g., a broadcast), the
	// packet will be dropped, and vice-versa. HOWEVER, incoming packets are currently not filtered. So,
	// if an XIA packet somehow arrives on an IP port, it will be processed as normal.

    // $local_addr: the full address of the node
	// $local_ad: the node's AD
	// $local_hid: the node's HID
    // $external_ip: the node's IP address (given to XHCP to give to connected hosts)  TODO: should eventually use all 4 individual external IPs
	// $fake: the fake interface apps use to communicate with this click element
	// $CLICK_IP: 
	// $API_IP: 
	// $ether_addr:
	// $ip_activeNUM:  1 = port NUM is connected to an IP network;  0 = port NUM is connected to an XIA network
	// $ipNUM:  port NUM's IP address (if port NUM isn't connected to IP, this doesn't matter)
	// $external_ipNUM:  port NUM's public IP address (might be different from $ipNUM if behind a NAT)
	// $macNUM:  port NUM's MAC address (if port NUM isn't connected to IP, this doesn't matter)
	// $gwNUM:  port NUM's gateway router's IP address (if port NUM isn't connected to IP, this doesn't matter)
	// $malicious_cache: if set to 1, the content cache responds with bad content

    // input[0], input[1], input[2], input[3]: a packet arrived at the node
    // output[0]: forward to interface 0
    // output[1]: forward to interface 1
    // output[2]: forward to interface 2
    // output[3]: forward to interface 3
    
	xrc :: XIARoutingCore($local_addr, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 4, 1);    


    Script(write xrc/n/proc/rt_AD.add $local_ad $DESTINED_FOR_LOCALHOST);    // self AD as destination
    Script(write xrc/n/proc/rt_IP.add IP:$ip0 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 0's IP addr
    Script(write xrc/n/proc/rt_IP.add IP:$ip1 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 1's IP addr
    Script(write xrc/n/proc/rt_IP.add IP:$ip2 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 2's IP addr
    Script(write xrc/n/proc/rt_IP.add IP:$ip3 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 3's IP addr
    Script(write xrc/n/proc/rt_IP.add - 3); 	// default route for IPv4     TODO: Need real routes somehow

    
	dlc0 :: XIADualLineCard($local_addr, $local_hid, $mac0, 0, $ip0, $gw0, $ip_active0);
	dlc1 :: XIADualLineCard($local_addr, $local_hid, $mac1, 1, $ip1, $gw1, $ip_active1);
	dlc2 :: XIADualLineCard($local_addr, $local_hid, $mac2, 2, $ip2, $gw2, $ip_active2);
	dlc3 :: XIADualLineCard($local_addr, $local_hid, $mac3, 3, $ip3, $gw3, $ip_active3);
    
    input => dlc0, dlc1, dlc2, dlc3 => output;
	xrc -> XIAPaintSwitch[0,1,2,3] => [1]dlc0[1], [1]dlc1[1], [1]dlc2[1], [1]dlc3[1] -> [0]xrc;
};

// 4-port dual stack router instrumented with counters and prints
elementclass XIAInstrumentedDualRouter4Port {
    $local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 
	$ip_active0, $ip0, $mac0, $gw0,
	$ip_active1, $ip1, $mac1, $gw1,
	$ip_active2, $ip2, $mac2, $gw2, 
	$ip_active3, $ip3, $mac3, $gw3 |

    wrapped :: XIADualRouter4Port($local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 
	$ip_active0, $ip0, $mac0, $gw0,
	$ip_active1, $ip1, $mac1, $gw1,
	$ip_active2, $ip2, $mac2, $gw2, 
	$ip_active3, $ip3, $mac3, $gw3);

    // TODO: Make XIAIPPrint element?

    print_in0 :: XIAPrint(">>> $local_hid (In Port 0)");
    print_in1 :: XIAPrint(">>> $local_hid (In Port 1)");
    print_in2 :: XIAPrint(">>> $local_hid (In Port 2)");
    print_in3 :: XIAPrint(">>> $local_hid (In Port 3)");
    print_out0 :: XIAPrint("<<< $local_hid (Out Port 0)");
    print_out1 :: XIAPrint("<<< $local_hid (Out Port 1)");
    print_out2 :: XIAPrint("<<< $local_hid (Out Port 2)");
    print_out3 :: XIAPrint("<<< $local_hid (Out Port 3)");

    // TODO: counter for inbound IP traffic

    count_final_out0 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out0 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);
    count_final_out1 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out1 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);
    count_final_out2 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out2 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);
    count_final_out3 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out3 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);

    input => print_in0, print_in1, print_in2, print_in3
        => wrapped 
        => print_out0, print_out1, print_out2, print_out3 
        => count_final_out0, count_final_out1, count_final_out2, count_final_out3
        => count_next_out0, count_next_out1, count_next_out2, count_next_out3 => output;
}

// 1-port endhost node with sockets
elementclass XIAEndHost {
    $local_addr, $local_hid, $fake, $CLICK_IP, $API_IP, $ether_addr, $enable_local_cache, $mac |

    // $local_addr: the full address of the node
    // $local_hid:  the HID of the node

    // input: a packet arrived at the node
    // output: forward to interface 0
    
	xrc :: XIARoutingCore($local_addr, $local_hid, 0.0.0.0, $fake, $CLICK_IP, $API_IP, $ether_addr, 1, 0);

    Script(write xrc/n/proc/rt_AD.add - 0);      // default route for AD
    Script(write xrc/n/proc/rt_IP.add - 0); 	// default route for IPv4    
    
	input -> xlc :: XIALineCard($local_addr, $local_hid, $mac, 0) -> output;
	xrc -> XIAPaintSwitch[0] -> [1]xlc[1] -> xrc;
};

// 1-port endhost node with sockets instrumented with printers and counters
elementclass XIAInstrumentedEndHost {
    $local_addr, $local_hid, $fake, $CLICK_IP, $API_IP, $ether_addr, $enable_local_cache, $mac |

    wrapped :: XIAEndHost($local_addr, $local_hid, $fake, $CLICK_IP, $API_IP, $ether_addr, $enable_local_cache, $mac);

    print_in0 :: XIAPrint(">>> $local_hid (In Port 0)");
    print_out0 :: XIAPrint("<<< $local_hid (Out Port 0)");

    count_final_out0 :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -);
    count_next_out0 :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -);

    input -> print_in0 -> wrapped -> print_out0 -> count_final_out0 -> count_next_out0 -> output;
};

// Endhost node with XRoute process running and IP support
elementclass XIADualEndhost {
    $local_addr, $local_ad, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 
	$ip_active0, $ip0, $mac0, $gw0,
	$malicious_cache |

	// NOTE: This router assumes that each port is connected to *either* an XIA network *or* an IP network.
	// If port 0 is connected to an IP network and is asked to send an XIA packet (e.g., a broadcast), the
	// packet will be dropped, and vice-versa. HOWEVER, incoming packets are currently not filtered. So,
	// if an XIA packet somehow arrives on an IP port, it will be processed as normal.

    // $local_addr: the full address of the node
	// $local_ad: the node's AD
	// $local_hid: the node's HID
    // $external_ip: the node's IP address (given to XHCP to give to connected hosts)  TODO: should eventually use all 4 individual external IPs
	// $fake: the fake interface apps use to communicate with this click element
	// $CLICK_IP: 
	// $API_IP: 
	// $ether_addr:
	// $ip_activeNUM:  1 = port NUM is connected to an IP network;  0 = port NUM is connected to an XIA network
	// $ipNUM:  port NUM's IP address (if port NUM isn't connected to IP, this doesn't matter)
	// $macNUM:  port NUM's MAC address (if port NUM isn't connected to IP, this doesn't matter)
	// $gwNUM:  port NUM's gateway router's IP address (if port NUM isn't connected to IP, this doesn't matter)
	// $malicious_cache: if set to 1, the content cache responds with bad content

    // input[0]: a packet arrived at the node
    // output[0]: forward to interface 0
    
	xrc :: XIARoutingCore($local_addr, $local_hid, $external_ip, $fake, $CLICK_IP, $API_IP, $ether_addr, 4, $malicious_cache, 1);    


    Script(write xrc/n/proc/rt_AD.add $local_ad $DESTINED_FOR_LOCALHOST);    // self AD as destination
    Script(write xrc/n/proc/rt_IP.add IP:$ip0 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 0's IP addr
    Script(write xrc/n/proc/rt_IP.add - 3); 	// default route for IPv4     TODO: Need real routes somehow

    
	dlc0 :: XIADualLineCard($local_addr, $local_hid, $mac0, 0, $ip0, $gw0, $ip_active0);
    
    input -> dlc0 -> output;
	xrc -> XIAPaintSwitch[0] => [1]dlc0[1] -> xrc;
};
