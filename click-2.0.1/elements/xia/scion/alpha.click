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


alpha_cert::SCIONCertServer(AID 33333, 
TOPOLOGY_FILE ./AD1/topology1.xml,
CONFIG_FILE ./AD1/AD1.conf, 
ROT_FILE ./ROT/ROT.xml);

alpha_switch::SCIONSwitch(CONFIG_FILE
"./AD1/AD1.conf", 
TOPOLOGY_FILE
"./AD1/topology1.xml");

alpha_pcb::SCIONBeaconServerCore(AID 11111, CONFIG_FILE
"./AD1/AD1.conf",TOPOLOGY_FILE ./AD1/topology1.xml);

alpha_router::SCIONRouter(AID 1111, CONFIG_FILE
"./AD1/AD1R1.conf", TOPOLOGY_FILE
"./AD1/topology1.xml");

pathservercore::SCIONPathServerCore(AID 22222, CONFIG ./AD1/AD1.conf);

rrswitch :: RoundRobinSwitch;

//arp :: ARPQuerier(192.168.0.1, en0);

// connect path/certificate/beacon server to switch

pathservercore[0]->[2]alpha_switch;
alpha_switch[2]->Queue(1000)->[0]pathservercore;

alpha_pcb->Print("PCB", 48)->[0]alpha_switch;
alpha_switch[0]->Queue(1000)->[0]alpha_pcb;

alpha_cert[0]->[3]alpha_switch;
alpha_switch[3]->Queue(1000)->[0]alpha_cert;

alpha_router[0]->[1]alpha_switch;
alpha_switch[1]->[0]alpha_router;

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
	-> Discard;


// handle SCION over IP datagrams
cl[2] 
	-> Strip(14) -> IPTunnelDecap  
    -> rrswitch;


cl[3] -> Print("Non-SCION-over-IP") -> Discard;

rrswitch[0] -> [1]alpha_router;
rrswitch[1] -> [2]alpha_router;
rrswitch[2] -> [3]alpha_router;


// send SCION packets into IP network
alpha_router[1] -> Queue -> [0]rrsched_ip :: RoundRobinSched;
alpha_router[2] -> Queue -> [1]rrsched_ip;
alpha_router[3] -> Queue -> [2]rrsched_ip;

// encapsulate and send
rrsched_ip
    -> Unqueue
    -> IPTunnelEncap(192.168.0.1, 192.168.0.2)
    -> EtherEncap(0x0800, en0, ff:ff:ff:ff:ff:ff)
    -> Queue
    -> [1]rrsched_out;

rrsched_out -> ToDevice(en0);
