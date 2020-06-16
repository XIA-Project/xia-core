require(library xia_constants.click);

elementclass XIAFromHost {
	$click_port |
	// Packets coming down from API
	// output: traffic from host (should go to [0]xtransport)
	Socket("UDP", 127.0.0.1, $click_port, SNAPLEN 65536) -> output;
};

elementclass XIAToHost {
	$click_port |
	// Packets to send up to API
	// input: packets to send up (usually xtransport[0])
	input -> Socket("UDP", 0.0.0.0, 0, SNAPLEN 65536);
};

elementclass CacheFilter {
	// Filter packets coming in from the network and if they contain
	//  CID data, tee the data off to the Xcache daemon as well as
	// forwarding it on.
	// The filter is not currently used on hosts but can be added if needed.
	//
	// the following command on the routers click control port can be
	//  used to change state
	//   write <hostname>/cf/cidFilter.enable true|false

	// Socket to forward packets to Xcache
	XcacheSock::Socket(UNIX_DGRAM, "/tmp/xcache-click.sock", CLIENT true,
		SNAPLEN 65536);
	// Socket to receive control requests from Xcache. e.g. blacklist
	CacheFilterSock::Socket(UNIX_DGRAM, "/tmp/cachefilter-click.sock",
		SNAPLEN 65536);

	srcCidClassifier::XIAXIDTypeClassifier(src CID, src NCID, -);

	cidFilter::XIACidFilter(true);	// if false, don't cache

	input -> srcCidClassifier;

	// just send non CID packets on
	srcCidClassifier[2] -> output;

	// All source CID packets are considered for caching. So,
	// sending one copy of CID out
	srcCidClassifier[0], srcCidClassifier[1] -> cidFork::Tee(2) -> output;
	// And the other to CIDfilter
	cidFork[1]->[1]cidFilter;

	// there is no useful info from xcache to the filter, we can remove this linkage at some point
	CacheFilterSock -> cidFilter -> XcacheSock;
};

elementclass GenericPostRouteProc {
	// output[0]: forward (decremented)
	// output[1]: hop limit reached (should send back a TTL expry packet)
	input -> XIADecHLIM[0,1] => output;
};

elementclass XIAPacketRoute {
	$num_ports |

	// input: a packet to process
	// output[0]: forward (painted)
	// output[1]: arrived at destination node
	// output[2]: could not route at all (tried all paths)
	// output[3]: SID hack for DHCP functionality

	// TO ADD A NEW USER DEFINED XID (step 1)
	// modify the following line by inserting the following text before the dash
	// next XID_NAME,
	//
	// order is important!
	//
	// c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, next IP, next FOO, -);

	// print_in :: XIAPrint(">>> $local_hid (In Port $num) ");
	// print_out :: XIAPrint("<<< $local_hid (Out Port $num)");

	c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, next IP, next FID, next NCID, next ICID, next AID, -);
	//print_in :: XIAPrint(">>> $local_hid (In Port $num) ");
	// pdesty :: XIAPrint(">>>  DEST YES");
	// pdestn :: XIAPrint(">>>  DEST NO");
	// p1 :: XIAPrint(">>> $local_hid (AD) ");
	// p2 :: XIAPrint(">>> $local_hid (HID) ");
	// p3 :: XIAPrint(">>> $local_hid (SID) ");
	// p4 :: XIAPrint(">>> $local_hid (CID) ");
	// p5 :: XIAPrint(">>> $local_hid (IP) ");
	// pwohoo :: XIAPrint(">>> $local_hid (WOOOOOOOOOOOOOOHHHHHHHHHHHHHHHH) ");
	// pgprp :: XIAPrint(">>> $local_hid (GPRPGPRP) ");
	// pnext :: XIAPrint(">>> $local_hid (NEXTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT) ");
	// pnext1 :: XIAPrint(">>> $local_hid (NEXTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT) ");
	// pdiscard :: XIAPrint(">>> $local_hid (DISCARRRRRRRRRRDDDDDDDDDDDDDDDDDDDD) ");
	// px :: XIAPrint(">>> $local_hid (XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX) ");

	input -> consider_first_path :: XIASelectPath(first);

	// arrived at the final destination or reiterate paths with new last pointer
	check_dest :: XIACheckDest() -> XIAPaint($DESTINED_FOR_LOCALHOST) -> [1]output
	check_dest[1] -> consider_first_path;

	consider_next_path :: XIASelectPath(next);
	consider_first_path => c, [2]output;
//	consider_first_path[0] -> c;
//	consider_first_path[1] -> [2]output;
	consider_next_path => c, [2]output;

	//NITIN disable XCMP REDIRECT messages
	x :: XCMP();
	x[1] -> Discard;
	GPRP :: GenericPostRouteProc[0] -> [0]output;
	//NITIN disable XCMP REDIRECT messages
	GPRP[1] -> x[0] -> consider_first_path;
	//GPRP[1] -> consider_first_path;

	// print_out :: XIAPrint("<<< $local_hid (Out Port $num)");
	// TO ADD A NEW USER DEFINED XID (step 2)
	// add rt_XID_NAME as the last entry in the list of rt_xxx in the following 2 lines
	// order is important!
	//
	// rt_AD, rt_HID, rt_SID, rt_CID, rt_IP, rt_FOO :: XIAXIDRouteTable($num_ports);
	// c => rt_AD, rt_HID, rt_SID, rt_CID, rt_IP, rt_FOO, [2]output;

	rt_AD, rt_HID, rt_SID, rt_IP, rt_AID:: XIAXIDRouteTable();
	rt_CID, rt_NCID :: XIACIDRouteTable();
	rt_ICID :: XIAICIDRouteTable();
	rt_FID :: FIDRouteEngine($num_ports);
	c => rt_AD, rt_HID, rt_SID, rt_CID, rt_IP, rt_FID, rt_NCID, rt_ICID, rt_AID, [2]output;

	// TO ADD A NEW USER DEFINED XID (step 3)
	// add rt_XID_NAME before the arrow in the following 7 lines
	// if the XID is used for routing like an AD or HID, add it to lines 1,2,4,5,7
	// if the XID should be treated like a SID and will return data to the API, add it to lines 1,3,4,6,7

	rt_AD[0], rt_HID[0], rt_SID[0], rt_CID[0], rt_IP[0], rt_FID[0], rt_NCID[0], rt_ICID[0], rt_AID[0] -> GPRP;
	rt_AD[1], rt_HID[1], 					   rt_IP[1], rt_FID[1]-> XIANextHop -> check_dest;
						 rt_SID[1], rt_CID[1]					  , rt_NCID[1], rt_ICID[1], rt_AID[1] -> XIANextHop -> XIAPaint($DESTINED_FOR_LOCALHOST) -> [1]output;
	rt_AD[2], rt_HID[2], rt_SID[2], rt_CID[2], rt_IP[2], rt_FID[2], rt_NCID[2], rt_ICID[2], rt_AID[2] -> consider_next_path;
	rt_AD[3], rt_HID[3],			rt_CID[3], rt_IP[3], rt_FID[3], rt_NCID[3], rt_ICID[3], rt_AID[3] -> Discard;
						 rt_SID[3]								  -> [3]output;
	//NITIN disable XCMP REDIRECT messages
	//NITIN rt_AD[4], rt_HID[4], rt_SID[4], rt_CID[4], rt_IP[4] rt_FID[4]-> x; // xcmp redirect message
};


elementclass RouteEngine {
	$num_ports |

	// input[0]: a packet arrived at the node from outside (i.e. routing with caching)
	// input[1]: a packet to send from a node (i.e. routing without caching)
	// output[0]: forward (painted)
	// output[1]: arrived at destination node; go to RPC
	// output[2]: arrived at destination node; go to cache

	proc :: XIAPacketRoute($num_ports);

	input[0] -> proc;
	input[1] -> proc;

	proc[0] -> [0]output;
	proc[1] -> [1]output;  // To RPC / Application

	proc[2] -> XIAPaint($UNREACHABLE) -> x::XCMP() -> proc;
	x[1] -> Discard;

	Idle() -> [2]output;

	// hack to use DHCP functionality
	proc[3] -> [3]output;
};

// Works at layer 2. Expects and outputs raw ethernet frames.
elementclass XIALineCard {
	$mac, $num, $ishost, $isrouter |

	// input[0]: a packet arriving from the network
	// input[1]: a packet arriving from the higher stack (i.e. router or end host)
	// output[0]: send out to network
	// output[1]: send up to the higher stack (i.e. router or end host)

	// setup XARP & XNetJoin modules
	//				XARP (query)	 XARP (response)  XIP	   XIANetJoin
	c :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/C0DE, 12/9991);
	xarpq :: XARPQuerier($mac);
	xarpr :: XARPResponder($mac);

//	print_in :: XIAPrint(">>> (In Iface $num) ");
	print_out :: XIAPrint("<<< (Out Iface $num)");

	count_final_out :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, dst FID, dst NCID, dst ICID, -);
	count_next_out :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, next FID, next NCID, next ICID, -);

	// AIP challenge-response HID verification module
	xchal :: XIAChallengeSource(INTERFACE $num, ACTIVE $isrouter);
	xresp :: XIAChallengeResponder(ACTIVE $ishost);

	// packets to network could be XIA packets or XARP queries (or XCMP messages?)
	// we only want to print/count the XIA packets
	toNet :: Tee(3) -> Queue(200) -> [0]output;   // send all packets
	toNet[1] -> statsFilter :: Classifier(12/C0DE, -) -> print_out -> count_final_out -> count_next_out -> Discard;  // only print/count XIP packets
	toNet[2] -> xrespFilter :: Classifier(12/C0DE, -) -> Strip(14) -> CheckXIAHeader -> MarkXIAHeader -> [1]xresp[2] -> Discard;
	statsFilter[1] -> Discard;   // don't print/count XARP or XCMP packets
	xrespFilter[1] -> Discard;

	// On receiving a packet from the host
	// If overlay packet, skip XARP for it
	input[1] -> overlayFilter::XIAOverlayFilter -> toNet;
	// Otherwise, regular XIA packet may need to XARP
	overlayFilter[1] -> xarpq;

	// On receiving a packet from interface
	// also, save the source port so we can use it in xtransport
	// XIA_FROMNET_ANNO is 1 on packets that arrived from network
	input[0] -> XIAPaint(ANNO $SRC_PORT_ANNO, COLOR $num) -> XIAPaint(ANNO $XIA_FROMNET_ANNO, COLOR 1) -> [1]overlayFilter[3] -> c;
    overlayFilter[2] -> CheckXIAHeader -> MarkXIAHeader -> [0]xchal;

	// Received a network joining packet
	c[3] -> xnetj :: XNetJ($mac) -> toNet;
	input[2] -> [1]xnetj[1] -> [2]output;

	// Receiving an XIA packet
	c[2] -> Strip(14) -> CheckXIAHeader -> MarkXIAHeader -> [0]xchal[0] -> [0]xresp[0] -> XIAPaint($num) -> [1]output;

	xchal[1] -> xarpq;
	xresp[1] -> CheckXIAHeader -> MarkXIAHeader -> XIAPaint($DESTINED_FOR_LOCALHOST) -> [1]output;

	// On receiving ARP response
	c[1] -> [1]xarpq -> toNet;

	// On receiving ARP query
	c[0] -> xarpr -> toNet;

	// XAPR timeout to XCMP
	xarpq[1] -> x :: XCMP() -> [1]output;
	x[1] -> Discard;
}

// Works at layer 3. Expects and outputs raw IP packets.
elementclass IPLineCard {
	$ip, $num, $mac, $gw |

	// input[0]: a packet arriving from the network
	// input[1]: a packet arriving from the higher stack (i.e. router or end host)
	// output[0]: send out to network
	// output[1]: send up to the higher stack (i.e. router or end host)

	// Set up ARP querier and responder
	//				ARP query		 ARP response	  IP	  UDP   4ID port
	c :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800 23/11 36/35d5);
	arpq :: ARPQuerier($ip, $mac);
	arpr :: ARPResponder($ip/32 $mac);

	print_in :: IPPrint(">>> $ip (In Port $num)");
	print_out :: XIAPrint("<<< $ip (Out Port $num)");

	// TODO: Make a counter for IP
	//count_final_out :: XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, dst FID, dst NCID, dst ICID, -);
	//count_next_out :: XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, next FID, next NCID, next ICID, -);

	toNet :: Null -> print_out -> Queue(200) -> [0]output; //count_final_out -> count_next_out -> [0]output;

	// On receiving a packet from the host
	// Sending an XIA-encapped-in-IP packet (via ARP if necessary)
	// If IP is in our subnet, do an ARP. Otherwise, send to the default gateway
	// dip sets the next IP annotation to the default gw so ARP knows what to query
	input[1] -> XIAIPEncap(SRC $ip) -> DirectIPLookup(0.0.0.0/0 $gw 0) -> arpq;

	// On receiving a packet from interface
	input[0] -> print_in -> c

	// Receiving an XIA-encapped-in-IP packet; strip the ethernet, IP, and UDP headers,
	// leaving bare XIP packet, then paint so we know which port it came in on
	c[2] -> Strip(14) -> CheckIPHeader() -> IPClassifier(dst host $ip and dst udp port 13781) -> MarkIPHeader -> StripIPHeader -> Strip(8) -> CheckXIAHeader -> MarkXIAHeader
	-> Paint($num) -> [1]output; // this should be send out to [0]n;

	// Receiving an ARP Response; return it to querier
	c[1] -> [1]arpq -> toNet;

	// Receiving an ARP Query; respond if it's interface's IP
	c[0] -> arpr -> toNet;
}

elementclass XIADualLineCard {
	$mac, $num, $ip , $gw, $ip_active, $ishost, $isrouter |

	// input[0]: a packet arriving from the network
	// input[1]: a packet arriving from the higher stack (i.e. router or end host)
	// output[0]: send out to network
	// output[1]: send up to the higher stack (i.e. router or end host)

	// 							   XARP query	   XARP response	XIP
	input[0] -> c :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/C0DE, -);

	toNet :: DRRSched -> [0]output;

	sup :: Suppressor;
	Script(write sup.active0 !$ip_active);
	Script(write sup.active1 $ip_active);

	c[0], c[1], c[2] -> xlc :: XIALineCard($mac, $num, $ishost, $isrouter) -> sup -> toNet;
	c[3] -> iplc :: IPLineCard($ip, $num, $mac, $gw) -> [1]sup[1] -> [1]toNet;

	// Packet needs forwarding and has been painted w/ output port;
	// check if it's heading to an XIA network or an IP network
	input[1] -> dstTypeC :: XIAXIDTypeClassifier(next IP, -);

	dstTypeC => [1]iplc[1], [1]xlc[1] -> [1]output;
}

elementclass XIARoutingCore {
	$hostname, $external_ip, $click_port, $num_ports, $is_dual_stack |

	// input[0]: packet to route
	// output[0]: packet to be forwarded out a given port based on paint value

	n :: RouteEngine($num_ports);

	// Socket to send ICID packets to Xcache
	//XcacheICIDSock::Socket(UNIX_DGRAM, "/tmp/xcache-icid.sock", CLIENT true,
		//SNAPLEN 65536);
	XcacheICIDSock::Socket(UDP, 127.0.0.1, 7994, CLIENT true,
		SNAPLEN 65536);

	xtransport::XTRANSPORT($hostname, IP:$external_ip, $num_ports, IS_DUAL_STACK_ROUTER $is_dual_stack);


	XIAFromHost($click_port) -> xtransport;
	xtransport[0] -> XIAToHost($click_port);

	Script(write n/proc/rt_HID.add - $FALLBACK);
	Script(write n/proc/rt_AD.add - $FALLBACK);	 // no default route for AD; consider other path
	Script(write n/proc/rt_SID.add - $FALLBACK);	 // no default route for SID; consider other path
	Script(write n/proc/rt_CID.add - $FALLBACK);	 // no default route for CID; consider other path
	Script(write n/proc/rt_IP.add - $FALLBACK);		// no default route for IP; consider other path
	Script(write n/proc/rt_FID.add - $DESTINED_FOR_BROADCAST);	// by default all FIDs are broadcast unless local
	Script(write n/proc/rt_NCID.add - $FALLBACK);		// no default route for NCID; consider other path
	Script(write n/proc/rt_ICID.add - $FALLBACK);		// no default route for ICID; consider other path

	// TO ADD A NEW USER DEFINED XID (step 4)
	// create a default fallback route for the new XID
	//
	// Script(write n/proc/rt_FOO.add - $FALLBACK);		// no default route for FOO; consider other path

	// quick fix
	n[3] -> Discard();

	// set up XCMP elements
	c :: Classifier(01/01, -); // XCMP
	x :: XCMP();

	n[0] -> output;
	input -> [0]n;

	// Send ICID packets to Xcache
	xtransport[2] -> XcacheICIDSock;

	n[1] -> c[1] -> [1]xtransport[1] -> XIAPaint($DESTINED_FOR_LOCALHOST) -> [0]n;
	c[0] -> x[0] -> [0]n; // new (response) XCMP packets destined for some other machine

	x[1] -> [1]xtransport; // XCMP packets destined for this machine
	n[2] -> [1]n; // Harshad dirty hack FIXME: Remove both ports

	// For get and put cid
}

elementclass XIAOverlayRouter {
	of0::XIAOverlayFilter();
	of1::XIAOverlayFilter();
	of2::XIAOverlayFilter();
	of3::XIAOverlayFilter();

	rd::XIAOverlayRouted();

	input -> rd;
	rd[0] -> of0 -> [0]output;
	rd[1] -> of1 -> [1]output;
	rd[2] -> of2 -> [2]output;
	rd[3] -> of3 -> [3]output;

	// discard remaining of ports
	of0[1]->[1]of0; 	of0[2]->Discard; 	of0[3]->Discard;
	of1[1]->[1]of1; 	of1[2]->Discard;	of1[3]->Discard;
	of2[1]->[1]of2;	of2[2]->Discard;	of2[3]->Discard;
	of3[1]->[1]of3;	of3[2]->Discard;	of3[3]->Discard;
}

// 2-port router
elementclass XIARouter2Port {
	$click_port, $hostname, $external_ip, $mac0, $mac1, |

	// $external_ip: an ingress IP address for this XIA cloud (given to hosts via XHCP)  TODO: be able to handle more than one

	// input[0], input[1]: a packet arrived at the node
	// output[0]: forward to interface 0
	// output[1]: forward to interface 1

	xrc :: XIARoutingCore($hostname, $external_ip, $click_port, 2, 0);

	xlc0 :: XIALineCard($mac0, 0, 0, 0);
	xlc1 :: XIALineCard($mac1, 1, 0, 0);

	cf :: CacheFilter;

	input => xlc0, xlc1 => output;
	xrc -> cf -> XIAPaintSwitch[0,1] => [1]xlc0[1], [1]xlc1[1]  -> [0]xrc;
};

// 4-port router node

elementclass XIARouter4Port {
	$click_port, $hostname, $external_ip,
	$mac0, $mac1, $mac2, $mac3 |

	xianetjoin :: XIANetJoin();

	// $external_ip: an ingress IP address for this XIA cloud (given to hosts via XHCP)  TODO: be able to handle more than one

	// input[0], input[1], input[2], input[3]: a packet arrived at the node
	// output[0]: forward to interface 0
	// output[1]: forward to interface 1
	// output[2]: forward to interface 2
	// output[3]: forward to interface 3

	xrc :: XIARoutingCore($hostname, $external_ip, $click_port, 4, 0);

	xlc0 :: XIALineCard($mac0, 0, 0, 0);
	xlc1 :: XIALineCard($mac1, 1, 0, 0);
	xlc2 :: XIALineCard($mac2, 2, 0, 0);
	xlc3 :: XIALineCard($mac3, 3, 0, 0);

	cf :: CacheFilter;

	input => xlc0, xlc1, xlc2, xlc3;
	xlc0, xlc1, xlc2, xlc3 => output;

	xrc -> cf -> XIAPaintSwitch[0,1,2,3] => [1]xlc0[1], [1]xlc1[1], [1]xlc2[1], [1]xlc3[1] -> [0]xrc;

	xianetjoin[0] -> XIAPaintSwitch[0,1,2,3] => [2]xlc0[2], [2]xlc1[2], [2]xlc2[2], [2]xlc3[2] -> [0]xianetjoin;
	XIAFromHost(9882) -> [1]xianetjoin[1] -> XIAToHost(9882);
};

// 8-port router node
elementclass XIARouter8Port {
	$click_port, $hostname, $external_ip,
	$mac0, $mac1, $mac2, $mac3, $mac4, $mac5, $mac6, $mac7 |

	xianetjoin :: XIANetJoin();

	// $external_ip: an ingress IP address for this XIA cloud (given to hosts via XHCP)  TODO: be able to handle more than one

	// input[0], input[1], input[2], input[3]: a packet arrived at the node
	// output[0]: forward to interface 0
	// output[1]: forward to interface 1
	// output[2]: forward to interface 2
	// output[3]: forward to interface 3

	xrc :: XIARoutingCore($hostname, $external_ip, $click_port, 4, 0);

	xlc0 :: XIALineCard($mac0, 0, 0, 0);
	xlc1 :: XIALineCard($mac1, 1, 0, 0);
	xlc2 :: XIALineCard($mac2, 2, 0, 0);
	xlc3 :: XIALineCard($mac3, 3, 0, 0);
	xlc4 :: XIALineCard($mac4, 4, 0, 0);
	xlc5 :: XIALineCard($mac5, 5, 0, 0);
	xlc6 :: XIALineCard($mac6, 6, 0, 0);
	xlc7 :: XIALineCard($mac7, 7, 0, 0);

	cf :: CacheFilter;

	input => xlc0, xlc1, xlc2, xlc3, xlc4, xlc5, xlc6, xlc7 => output;
	xrc -> cf -> XIAPaintSwitch[0,1,2,3,4,5,6,7] => [1]xlc0[1], [1]xlc1[1], [1]xlc2[1], [1]xlc3[1],
													[1]xlc4[1], [1]xlc5[1], [1]xlc6[1], [1]xlc7[1]  -> [0]xrc;

	xianetjoin[0] -> XIAPaintSwitch[0,1,2,3,4,5,6,7] => [2]xlc0[2], [2]xlc1[2], [2]xlc2[2], [2]xlc3[2],
														[2]xlc4[2], [2]xlc5[2], [2]xlc6[2], [2]xlc7[2] -> [0]xianetjoin;
	XIAFromHost(9882) -> [1]xianetjoin[1] -> XIAToHost(9882);
};

// 4-port router node with XRoute process running and IP support
elementclass XIADualRouter4Port {
	$click_port, $hostname, $local_ad, $external_ip,
	$click_in_port, $click_out_port,
	$ip_active0, $ip0, $mac0, $gw0,
	$ip_active1, $ip1, $mac1, $gw1,
	$ip_active2, $ip2, $mac2, $gw2,
	$ip_active3, $ip3, $mac3, $gw3 |

	// NOTE: This router assumes that each port is connected to *either* an XIA network *or* an IP network.
	// If port 0 is connected to an IP network and is asked to send an XIA packet (e.g., a broadcast), the
	// packet will be dropped, and vice-versa. HOWEVER, incoming packets are currently not filtered. So,
	// if an XIA packet somehow arrives on an IP port, it will be processed as normal.

	// $local_ad: the node's AD
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

	// input[0], input[1], input[2], input[3]: a packet arrived at the node
	// output[0]: forward to interface 0
	// output[1]: forward to interface 1
	// output[2]: forward to interface 2
	// output[3]: forward to interface 3

	xrc :: XIARoutingCore($hostname, $external_ip, $click_port, 4, 1);


	Script(write xrc/n/proc/rt_AD.add $local_ad $DESTINED_FOR_LOCALHOST);	// self AD as destination
	Script(write xrc/n/proc/rt_IP.add IP:$ip0 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 0's IP addr
	Script(write xrc/n/proc/rt_IP.add IP:$ip1 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 1's IP addr
	Script(write xrc/n/proc/rt_IP.add IP:$ip2 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 2's IP addr
	Script(write xrc/n/proc/rt_IP.add IP:$ip3 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 3's IP addr
	Script(write xrc/n/proc/rt_IP.add - 3); 	// default route for IPv4	 TODO: Need real routes somehow


	dlc0 :: XIADualLineCard($mac0, 0, $ip0, $gw0, $ip_active0, 0, 0);
	dlc1 :: XIADualLineCard($mac1, 1, $ip1, $gw1, $ip_active1, 0, 0);
	dlc2 :: XIADualLineCard($mac2, 2, $ip2, $gw2, $ip_active2, 0, 0);
	dlc3 :: XIADualLineCard($mac3, 3, $ip3, $gw3, $ip_active3, 0, 0);

	cf :: CacheFilter;

	input => dlc0, dlc1, dlc2, dlc3 => output;
	xrc -> cf -> XIAPaintSwitch[0,1,2,3] => [1]dlc0[1], [1]dlc1[1], [1]dlc2[1], [1]dlc3[1] -> [0]xrc;
};

// endhost node with sockets
elementclass XIAEndHost {
	$click_port, $hostname, $mac0, $mac1, $mac2, $mac3 |

	xianetjoin :: XIANetJoin();

	// input: a packet arrived at the node
	// output: forward to interface 0

	xrc :: XIARoutingCore($hostname, 0.0.0.0, $click_port, 4, 0);

	Script(write xrc/n/proc/rt_AD.add - 0);	  // default route for AD
	Script(write xrc/n/proc/rt_IP.add - 0); 	// default route for IPv4
	Script(write xrc/n/proc/rt_HID.add - 0); 	// default route for HID (so hosts can reach other hosts on the same AD)

	xlc0 :: XIALineCard($mac0, 0, 0, 0);
	xlc1 :: XIALineCard($mac1, 1, 0, 0);
	xlc2 :: XIALineCard($mac2, 2, 0, 0);
	xlc3 :: XIALineCard($mac3, 3, 0, 0);

	input => xlc0, xlc1, xlc2, xlc3 => output;
	xrc -> XIAPaintSwitch[0,1,2,3] => [1]xlc0[1], [1]xlc1[1], [1]xlc2[1], [1]xlc3[1] -> [0]xrc;

	xianetjoin[0] -> XIAPaintSwitch[0,1,2,3] => [2]xlc0[2], [2]xlc1[2], [2]xlc2[2], [2]xlc3[2] -> [0]xianetjoin;
	XIAFromHost(9882) -> [1]xianetjoin[1] -> XIAToHost(9882);
};

// Endhost node with XRoute process running and IP support
elementclass XIADualEndhost {
	$local_ad, $external_ip, $click_port, $hostname,
	$ip_active0, $ip0, $mac0, $gw0 |

	// NOTE: This router assumes that each port is connected to *either* an XIA network *or* an IP network.
	// If port 0 is connected to an IP network and is asked to send an XIA packet (e.g., a broadcast), the
	// packet will be dropped, and vice-versa. HOWEVER, incoming packets are currently not filtered. So,
	// if an XIA packet somehow arrives on an IP port, it will be processed as normal.

	// $local_ad: the node's AD
	// $external_ip: the node's IP address (given to XHCP to give to connected hosts)  TODO: should eventually use all 4 individual external IPs
	// $fake: the fake interface apps use to communicate with this click element
	// $CLICK_IP:
	// $API_IP:
	// $ether_addr:
	// $ip_activeNUM:  1 = port NUM is connected to an IP network;  0 = port NUM is connected to an XIA network
	// $ipNUM:  port NUM's IP address (if port NUM isn't connected to IP, this doesn't matter)
	// $macNUM:  port NUM's MAC address (if port NUM isn't connected to IP, this doesn't matter)
	// $gwNUM:  port NUM's gateway router's IP address (if port NUM isn't connected to IP, this doesn't matter)

	// input[0]: a packet arrived at the node
	// output[0]: forward to interface 0

	xrc :: XIARoutingCore($hostname, $external_ip, $click_port, 4, 1);


	Script(write xrc/n/proc/rt_AD.add $local_ad $DESTINED_FOR_LOCALHOST);	// self AD as destination
	Script(write xrc/n/proc/rt_IP.add IP:$ip0 $DESTINED_FOR_LOCALHOST);  // self as destination for interface 0's IP addr
	Script(write xrc/n/proc/rt_IP.add - 3); 	// default route for IPv4	 TODO: Need real routes somehow


	dlc0 :: XIADualLineCard($mac0, 0, $ip0, $gw0, $ip_active0, 1, 0);

	input -> dlc0 -> output;
	xrc -> XIAPaintSwitch[0] => [1]dlc0[1] -> xrc;
};
