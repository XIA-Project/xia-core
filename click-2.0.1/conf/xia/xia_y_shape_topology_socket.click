require(library xia_router_template_xudp.click);
require(library xia/xia_address.click);

// host & router instantiation
host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,0);
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1);
host2 :: EndHost (RE AD1 HID2, HID2, fake2,212.0.0.2,212.0.0.1,31:11:11:11:11:11,1);

router0 :: Router(RE AD0 RHID0, AD0, RHID0);
//router1 :: Router(RE AD1 RHID1, AD1, RHID1);
router1 :: Router4Port(RE AD1 RHID1);

// interconnection -- host - ad
host0[0] ->  XIAPrint("h0->r0") -> c::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)-> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> XIAPrint("r0->h0")-> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

host1[0] -> XIAPrint("h1->r1")-> LinkUnqueue(0.005, 1 GB/s) 
-> bc::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -) 
-> [0]router1;

router1[0]-> XIAPrint("r1->h1")->  LinkUnqueue(0.005, 1 GB/s) ->[0]host1;

host2[0] -> XIAPrint("h2->r1")-> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;
router1[1]-> XIAPrint("r1->h2")->  LinkUnqueue(0.005, 1 GB/s) ->[0]host2;

Idle -> [3]router1[3] -> Discard;

// interconnection -- ad - ad
router0[1] ->  XIAPrint("r0->r1")  -> LinkUnqueue(0.005, 1 GB/s) ->[2]router1;
router1[2] ->  XIAPrint("r1->r0")  -> LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

ControlSocket(tcp, 7000);


Script(write router1/n/proc/rt_AD/rt.add - 2);  // default route for AD
Script(write router1/n/proc/rt_AD/rt.add AD1 4);  // self AD as destination

Script(write router1/n/proc/rt_HID/rt.add HID1 0);  // forwarding for local HID1
Script(write router1/n/proc/rt_HID/rt.add HID2 1);  // forwarding for local HID2
Script(write router1/n/proc/rt_HID/rt.add RHID1 4);  // self HID as destination
  
Script(write router1/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
Script(write router1/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path




