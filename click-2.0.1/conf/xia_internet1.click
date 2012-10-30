require(library xia_router_template_xudp.click);
require(library xia_address.click);

// host & router instantiation
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,0);
router1 :: DualRouter(RE AD1 RHID1, AD1, RHID1, HID1, eth0, eth0);

// interconnection -- host - ad
host1[0] ->  Script(TYPE PACKET, print "host1 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] ->  Script(TYPE PACKET, print "router1 output0", print_realtime) -> LinkUnqueue(0.005, 1 GB/s) -> [0]host1;

// interconnection -- ad - ad
router1[1] -> ToDevice(eth0);
FromDevice(eth0) -> [1]router1;