require(library xia_router_template_xudp.click);
require(library xia_address.click);

// host & router instantiation
host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,1);
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,0);
//host0 :: Host(RE AD0 HID0, HID0, 2000);
//host1 :: Host(RE AD1 HID1, HID1, 2001);
router0 :: Router(RE AD0 RHID0, AD0, RHID0);
router1 :: Router(RE AD1 RHID1, AD1, RHID1);

// interconnection -- host - ad
host0[0] ->  Script(TYPE PACKET, print "host0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] ->  Script(TYPE PACKET, print "router0 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

host1[0] ->  Script(TYPE PACKET, print "host1 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] ->  Script(TYPE PACKET, print "router1 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) ->[0]host1;

// interconnection -- ad - ad
router0[1] ->  Script(TYPE PACKET, print "router0 output1", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) ->[1]router1;
router1[1] ->  Script(TYPE PACKET, print "router1 output1", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

// send test packets from host0 to host1
/*
gen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
-> RatedUnqueue(5)
-> XIAEncap(
    DST RE AD1 HID1,
    SRC RE AD0 HID0)
-> AggregateCounter(COUNT_STOP 1)
-> host0;
*/
// send test packets from host1 to host0
//gen :: InfiniteSource(LENGTH 100, ACTIVE false, HEADROOM 256)
//-> RatedUnqueue(5)
//-> XIAEncap(
//    DST RE AD0 HID0,
//    SRC RE AD1 HID1)
//-> AggregateCounter(COUNT_STOP 1)
//-> host1;

//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed

