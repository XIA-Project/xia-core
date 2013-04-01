// topology
//
// +---...-------------------------+                          +---------------+
// |   ...                         |                          |               |
// |   ...   switch <-----> router |  <-----> gateway <-----> |       IP      |
// |   ...                         |                          |               |
// |   ...        (SCION)          |                          |               |
// +---...-------------------------+                          +---------------+

elementclass IPTunnelEncap {
    $src, $dst |
    input -> IPEncap(0xFE, $src, $dst) -> CheckIPHeader -> GetIPAddress(0) -> IPFragmenter(1500) -> output;
}

elementclass IPTunnelDecap {
    CheckIPHeader -> IPReassembler -> CheckIPHeader -> StripIPHeader;
}

beta_cert::SCIONCertServer(AID 33333, 
TOPOLOGY_FILE ./AD2/topology2.xml,
CONFIG_FILE ./AD2/AD2.conf, 
ROT_FILE ./ROT/ROT.xml);

beta_switch::SCIONSwitch(CONFIG_FILE
"./AD2/AD2.conf", 
TOPOLOGY_FILE
"./AD2/topology2.xml");

beta_pcb::SCIONBeaconServer(AID 11111, CONFIG_FILE
"./AD2/AD2.conf", TOPOLOGY_FILE "./AD2/topology2.xml", ROT_FILE ./ROT/ROT.xml);

beta_router::SCIONRouter(AID 1111, CONFIG_FILE
"./AD2/AD2R1.conf", TOPOLOGY_FILE
"./AD2/topology2.xml");

rrswitch :: RoundRobinSwitch;

//arp :: ARPQuerier(192.168.0.2, en0);

// connect path/certificate/beacon server to switch

beta_pcb->[1]beta_switch;
beta_switch[1]->Queue->Print("PCB", 48)->[0]beta_pcb;

beta_cert[0]->[2]beta_switch;
beta_switch[2]->Queue->[0]beta_cert;

beta_router[0]->[0]beta_switch;
beta_switch[0]->[0]beta_router;

// classify incoming packets

FromDevice(en0)
	-> HostEtherFilter(en0)
	-> cl :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800 23/FE, -)

// handle ARP request
    -> ARPResponder(192.168.0.1 en0) 
    -> Queue(1000)
    -> [0]rrsched_out :: RoundRobinSched;


// handle ARP reply
cl[1] 
	-> CheckARPHeader(14, VERBOSE true)
//      -> [1]arp;
	-> Discard;


// handle SCION over IP datagrams
cl[2] 
	-> Strip(14) -> CheckIPHeader -> IPReassembler -> CheckIPHeader -> StripIPHeader  
	-> rrswitch;

// otherwise
cl[3] -> Print("Non-SCION-over-IP")->Discard;

rrswitch[0] -> [1]beta_router;
rrswitch[1] -> [2]beta_router;
rrswitch[2] -> [3]beta_router;


// send SCION packets into IP network
beta_router[1] -> Queue -> [0]rrsched_ip :: RoundRobinSched;
beta_router[2] -> Queue -> [1]rrsched_ip;
beta_router[3] -> Queue -> [2]rrsched_ip;

// encapsulate and send
rrsched_ip
    -> Unqueue
    -> IPEncap(0xFE, 192.168.0.1, 192.168.0.2) 
    -> CheckIPHeader
    -> GetIPAddress(0)
    -> IPFragmenter(1500)
//    -> [0]arp[0]
    -> EtherEncap(0x0800, en0, ff:ff:ff:ff:ff:ff)
    -> Queue
    -> [1]rrsched_out;

rrsched_out -> ToDevice(en0);
