require(library xia_router_template_xtransport.click);
require(library xia_address.click);

// host & router instantiation
host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,0);
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1);
//host0 :: Host(RE AD0 HID0, HID0, 2000);
//host1 :: Host(RE AD1 HID1, HID1, 2001);
router0 :: Router(RE AD0 RHID0, AD0, RHID0);
router1 :: Router(RE AD1 RHID1, AD1, RHID1);

// interconnection -- host - ad
host0[0] ->  XIAPrint("host0->router0") -> c::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)-> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;

router0[0] -> XIAPrint("router0->host0") -> LinkUnqueue(0.005, 1 GB/s) -> RandomSample(1.0)-> [0]host0;

//RandomSample(1)->

host1[0] -> XIAPrint("host1->router1") -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;

router1[0] -> XIAPrint("router1->host1") -> LinkUnqueue(0.005, 1 GB/s)->  [0]host1;

// interconnection -- ad - ad
router0[1] ->  XIAPrint("router0->router1") -> LinkUnqueue(0.005, 1 GB/s) ->[1]router1;

router1[1] ->  XIAPrint("router1->router0") -> LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

ControlSocket(tcp, 7777);
