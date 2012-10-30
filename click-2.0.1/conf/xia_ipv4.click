require(library xia_router_template_xudp.click);
require(library xia_address.click);
require(library xia_simple_ip_router.click);

// eth0 18.26.4.92 00:00:C0:3B:71:EF
// eth1 1.0.0.1 00:00:C0:CA:68:EF

// host & router instantiation
host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,1);
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,0);
router0 :: DualRouter(RE AD0 RHID0, AD0, RHID0, HID0, 18.26.4.30, 00:00:C0:3B:71:EE);
router1 :: DualRouter(RE AD1 RHID1, AD1, RHID1, HID1, 1.0.0.2, 00:00:C0:Ca:68:EE);
iprouter :: IPRouter;

// interconnection -- host - ad
host0[0] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] ->  Script(TYPE PACKET, print "router0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

host1[0] ->  Script(TYPE PACKET, print "host1 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] ->  Script(TYPE PACKET, print "router1 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]host1;

// interconnection -- ad - ad
router0[1] -> Script(TYPE PACKET, print "router0 output1", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]iprouter;
router1[1] -> Script(TYPE PACKET, print "router1 output1", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [1]iprouter;
iprouter[0] -> Script(TYPE PACKET, print "iprouter output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [1]router0;
iprouter[1] -> Script(TYPE PACKET, print "iprouter output1", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;

/*
// send test packets from host0 to host1
ipgen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
    SRC RE AD0 HID0,
    DST DAG			0 1 -	// -1
    	       AD1	       	2 -	// 0
	       IPID1 		2 -	// 1
	       HID1		  	// 2
        , DYNAMIC false		) 
-> AggregateCounter(COUNT_STOP 2)
-> host0;


Script(write ipgen.active true);*/