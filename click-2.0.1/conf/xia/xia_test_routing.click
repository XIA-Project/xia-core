require(library xia_router_template_xtransport.click);
require(library xia_address.click);

// host & router instantiation
host_0a :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,01:11:11:11:11:11,0);
host_0b :: EndHost (RE AD0 HID1, HID1, fake1,195.0.0.2,195.0.0.1,11:11:11:11:11:11,1);
router0 :: XRouter4Port(RE AD0 RHID0, AD0, RHID0, fake2, 182.0.0.2, 182.0.0.1, 21:11:11:11:11:11);

host_1a :: EndHost (RE AD1 HID2, HID2, fake3,173.0.0.2,173.0.0.1,31:11:11:11:11:11,0);
host_1b :: EndHost (RE AD1 HID3, HID3, fake4,193.0.0.2,193.0.0.1,41:11:11:11:11:11,1);
router1 :: XRouter4Port(RE AD1 RHID1, AD1, RHID1, fake5, 183.0.0.2, 183.0.0.1, 51:11:11:11:11:11);

host_2a :: EndHost (RE AD2 HID4, HID4, fake6,175.0.0.2,175.0.0.1,61:11:11:11:11:11,0);
host_2b :: EndHost (RE AD2 HID5, HID5, fake7,192.0.0.2,192.0.0.1,71:11:11:11:11:11,1);
router2 :: XRouter4Port(RE AD2 RHID2, AD2, RHID2, fake8, 185.0.0.2, 185.0.0.1, 81:11:11:11:11:11);

router3 :: XRouter4Port(RE AD3 RHID3, AD3, RHID3, fake9, 186.0.0.2, 186.0.0.1, 91:11:11:11:11:11);


// interconnection -- hosts - ad0
host_0a[0] ->  XIAPrint("host_0a->router0") -> c::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)-> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> XIAPrint("router0->host_0a") -> LinkUnqueue(0.005, 1 GB/s) -> RandomSample(1.0)-> [0]host_0a;

host_0b[0] ->  XIAPrint("host_0b->router0") -> LinkUnqueue(0.005, 1 GB/s) -> [1]router0;
router0[1] -> XIAPrint("router0->host_0b") -> LinkUnqueue(0.005, 1 GB/s) -> RandomSample(1.0)-> [0]host_0b;

// interconnection -- hosts - ad1
host_1a[0] ->  XIAPrint("host_1a->router1") -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] -> XIAPrint("router1->host_1a") -> LinkUnqueue(0.005, 1 GB/s) -> RandomSample(1.0)-> [0]host_1a;

host_1b[0] ->  XIAPrint("host_1b->router1") -> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;
router1[1] -> XIAPrint("router1->host_1b") -> LinkUnqueue(0.005, 1 GB/s) -> RandomSample(1.0)-> [0]host_1b;

// interconnection -- hosts - ad2
host_2a[0] ->  XIAPrint("host_2a->router2") -> LinkUnqueue(0.005, 1 GB/s) -> [0]router2;
router2[0] -> XIAPrint("router2->host_1a") -> LinkUnqueue(0.005, 1 GB/s) -> RandomSample(1.0)-> [0]host_2a;

host_2b[0] ->  XIAPrint("host_2b->router2") -> LinkUnqueue(0.005, 1 GB/s) -> [1]router2;
router2[1] -> XIAPrint("router2->host_2b") -> LinkUnqueue(0.005, 1 GB/s) -> RandomSample(1.0)-> [0]host_2b;




// interconnection -- ad0 - ad3
router0[2] ->  XIAPrint("router0->router3") -> LinkUnqueue(0.005, 1 GB/s) ->[0]router3;
router3[0] ->  XIAPrint("router3->router0") -> LinkUnqueue(0.005, 1 GB/s) ->[2]router0;

// interconnection -- ad1 - ad3
router1[2] ->  XIAPrint("router1->router3") -> LinkUnqueue(0.005, 1 GB/s) ->[1]router3;
router3[1] ->  XIAPrint("router3->router1") -> LinkUnqueue(0.005, 1 GB/s) ->[2]router1;

// interconnection -- ad2 - ad3
router2[2] ->  XIAPrint("router2->router3") -> LinkUnqueue(0.005, 1 GB/s) ->[2]router3;
router3[2] ->  XIAPrint("router3->router2") -> LinkUnqueue(0.005, 1 GB/s) ->[2]router2;


// unused router ports...
Idle -> [3]router0[3] -> Discard;
Idle -> [3]router1[3] -> Discard;
Idle -> [3]router2[3] -> Discard;
Idle -> [3]router3[3] -> Discard;

ControlSocket(tcp, 7777);


// Route table setup for router0
//Script(write router0/n/proc/rt_AD/rt.add - 2);  // default route for AD
//Script(write router0/n/proc/rt_AD/rt.add AD0 4);  // self AD as destination

Script(write router0/n/proc/rt_HID/rt.add HID0 0);  // forwarding for local HID1
Script(write router0/n/proc/rt_HID/rt.add HID1 1);  // forwarding for local HID2
//Script(write router0/n/proc/rt_HID/rt.add RHID0 4);  // self HID as destination
  
//Script(write router0/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write router0/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path


// Route table setup for router1
//Script(write router1/n/proc/rt_AD/rt.add - 2);  // default route for AD
//Script(write router1/n/proc/rt_AD/rt.add AD1 4);  // self AD as destination

Script(write router1/n/proc/rt_HID/rt.add HID2 0);  // forwarding for local HID2
Script(write router1/n/proc/rt_HID/rt.add HID3 1);  // forwarding for local HID3
//Script(write router1/n/proc/rt_HID/rt.add RHID1 4);  // self HID as destination
  
//Script(write router1/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write router1/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path


// Route table setup for router2
//Script(write router2/n/proc/rt_AD/rt.add - 2);  // default route for AD
//Script(write router2/n/proc/rt_AD/rt.add AD2 4);  // self AD as destination

Script(write router2/n/proc/rt_HID/rt.add HID4 0);  // forwarding for local HID4
Script(write router2/n/proc/rt_HID/rt.add HID5 1);  // forwarding for local HID5
//Script(write router2/n/proc/rt_HID/rt.add RHID2 4);  // self HID as destination
  
//Script(write router2/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write router2/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path


// Route table setup for router3
//Script(write router3/n/proc/rt_AD/rt.add AD3 4);  // self AD as destination
//Script(write router3/n/proc/rt_AD/rt.add AD0 0);  // forwarding for AD0	
//Script(write router3/n/proc/rt_AD/rt.add AD1 1);  // forwarding for AD1	
//Script(write router3/n/proc/rt_AD/rt.add AD2 2);  // forwarding for AD2	
  
//Script(write router3/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write router3/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path






