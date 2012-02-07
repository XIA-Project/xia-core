elementclass XIAPacketRoute {
    // input: a packet to process
    // output[0]: forward (painted)
    // output[1]: arrived at destination node
    // output[2]: could not route at all (tried all paths)

    check_dest :: XIACheckDest();
    consider_first_path :: XIASelectPath(first);
    consider_next_path :: XIASelectPath(next);
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, next IP, -);

    //input -> Print("packet received by $local_addr") -> consider_first_path;
    input -> consider_first_path;

    check_dest[0] -> [1]output;             // arrived at the final destination
    check_dest[1] -> consider_first_path;   // reiterate paths with new last pointer

    consider_first_path[0] -> c;
    consider_first_path[1] -> [2]output;
    consider_next_path[0] -> c;
    consider_next_path[1] -> [2]output;

    //  Next destination is AD
    c[0] -> rt_AD :: GenericRouting4Port;
    rt_AD[0] -> GenericPostRouteProc -> [0]output;
    rt_AD[1] -> XIANextHop -> check_dest;
    rt_AD[2] -> consider_next_path;

    //  Next destination is HID
    c[1] -> rt_HID :: GenericRouting4Port;
    rt_HID[0] -> GenericPostRouteProc -> [0]output;
    rt_HID[1] -> XIANextHop -> check_dest;
    rt_HID[2] -> consider_next_path;

    //  Next destination is SID
    c[2] -> rt_SID :: GenericRouting4Port;
    rt_SID[0] -> GenericPostRouteProc -> [0]output;
    rt_SID[1] -> XIANextHop -> check_dest;
    rt_SID[2] -> consider_next_path;

    // change this if you want to do CID post route processing for any reason
    CIDPostRouteProc :: Null;

    //  Next destination is CID
    c[3] -> rt_CID :: GenericRouting4Port;
    rt_CID[0] -> GenericPostRouteProc -> CIDPostRouteProc -> [0]output;
    rt_CID[1] -> XIANextHop -> check_dest;
    rt_CID[2] -> consider_next_path;


    // Next destination is an IPv4 path
    c[4] -> rt_IP :: GenericRouting4Port;
    rt_IP[0] -> GenericPostRouteProc -> [0]output;
    rt_IP[1] -> XIANextHop -> check_dest;
    rt_IP[2] -> consider_next_path;

    c[5] -> [2]output;
};

elementclass RouteEngine {
    $local_addr |

    // $local_addr: the full address of the node (only used for debugging)

    // input[0]: a packet arrived at the node from outside (i.e. routing with caching)
    // input[1]: a packet to send from a node (i.e. routing without caching)
    // output[0]: forward (painted)
    // output[1]: arrived at destination node; go to RPC
    // output[2]: arrived at destination node; go to cache

    srcTypeClassifier :: XIAXIDTypeClassifier(src CID, -);
    proc :: XIAPacketRoute($local_addr);
    dstTypeClassifier :: XIAXIDTypeClassifier(dst CID, -);

    input[0] -> srcTypeClassifier;
    input[1] -> proc;

    srcTypeClassifier[0] -> cidFork :: Tee(2) -> [2]output;  // To cache (for content caching)
    cidFork[1] -> proc;                 // Main routing process

    srcTypeClassifier[1] -> proc;       // Main routing process

    proc[0] -> [0]output;               // Forward to other interface

    proc[1] -> dstTypeClassifier;
    dstTypeClassifier[1] -> [1]output;  // To RPC

    dstTypeClassifier[0] ->[2]output;  // To cache (for serving content request)

    proc[2] -> XIAPrint("Drop") -> Discard;  // No route drop (future TODO: return an error packet)
};

elementclass XIAHost {
  $eth_fake_name, $eth_addr, $CLICK_IP, $API_IP, $xia_pub_key |

  // elements
  eth_fake :: FromHost($eth_fake_name,
                       $API_IP/24,
                       CLICK_XTRANSPORT_ADDR $CLICK_IP);
  eth_fake_classifier :: Classifier(12/0806,
                                    12/0800);
  xia_api_sorter :: IPClassifier(dst udp port 5001,
                                 dst udp port 10001);
  xtransport :: XTRANSPORT2($CLICK_IP,
                            $API_IP,
                            n/proc/rt_SID/rt);
  route_engine :: XIARouteEngine();

  // static routing configuration
  Script(write route_engine/proc/rt_AD/rt.add - 0);
  Script(write route_engine/proc/rt_HID/rt.add - 0);
  Script(write route_engine/proc/rt_HID/rt.add HID:1111111111111111111111111111111111111111 4);
  Script(write route_engine/proc/rt_SID/rt.add - 5);
  Script(write route_engine/proc/rt_CID/rt.add - 5);
  Script(write route_engine/proc/rt_IP/rt.add - 0);

  // APPLICATION SIDE
  eth_fake -> eth_fake_classifier;
  eth_fake_classifier[0] -> ARPResponder(0.0.0.0 $ether_addr) -> ToHost($eth_fake_name);
  eth_fake_classifier[1] -> xia_api_sorter;
  xia_api_sorter[0] -> [0]xtransport;               // control packet
  xia_api_sorter[1] -> [1]xtransport;               // data packet
  xtransport[0] -> Discard();                       // port 0 unused 
  xtransport[1] -> EtherEncap(0x0800, $eth_addr, 11:11:11:11:11:11) -> ToHost($eth_fake_name);

  // NETWORK SIDE
  input -> [1]route_engine[1] -> Queue(200) -> output;
}
