// DEPRECATED!!!!

require(library xia_router_template_xtransport.click);
require(library xia_address.click);

// host & router instantiation
host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,0,31:11:11:11:11:11);
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1,41:11:11:11:11:11);
//host0 :: Host(RE AD0 HID0, HID0, 2000);
//host1 :: Host(RE AD1 HID1, HID1, 2001);
router0 :: XRouter2Port(RE AD0 RHID0, AD0, RHID0,);
router1 :: XRouter2Port(RE AD1 RHID1, AD1, RHID1);

// interconnection -- host - ad
host0[0] ->  XIAPrint("h0->r0") -> c::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)-> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> XIAPrint("r0->h0")-> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

host1[0] -> XIAPrint("h1->r1")-> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0]-> XIAPrint("r1->h1")->  LinkUnqueue(0.005, 1 GB/s) ->[0]host1;

// interconnection -- ad - ad
router0[1] ->  XIAPrint("r0->r1")  -> LinkUnqueue(0.005, 1 GB/s) ->[1]router1;
router1[1] ->  XIAPrint("r1->r0")  -> LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

ControlSocket(tcp, 7777);

/*ping :: XCMPPingSource(SRC RE AD0 HID0, DST RE AD1 HID1)
-> RatedUnqueue(5)
-> AggregateCounter(COUNT_STOP 5)
-> host0;*/

