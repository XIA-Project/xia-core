require(library xia_router_template_xudp.click);
require(library xia_address.click);

// host & router instantiation
host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,1);
router0 :: DualRouter(RE AD0 RHID0, AD0, RHID0, HID0, eth0, eth0);

// interconnection -- host - ad
host0[0] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] ->  Script(TYPE PACKET, print "router0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

// interconnection -- ad - ad
router0[1] -> ToDevice(eth0);

FromDevice(eth0) -> [1]router0;

XIAXIDInfo(
IPID2 IP:128.2.208.167
);

// send test packets from host0 to host1
ipgen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
    SRC RE AD0 HID0,
    DST DAG			0 1 -	// -1
    	       AD1	       	2 -	// 0
	       IPID2 		2 -	// 1
	       HID1		  	// 2
        , DYNAMIC false		) 
-> AggregateCounter(COUNT_STOP 2)
-> host0;


//Script(write ipgen.active true);