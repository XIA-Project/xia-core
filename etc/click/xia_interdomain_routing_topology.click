require(library xia_router_lib.click);
require(library xia_address.click);

// host & router instantiation
host0 :: XIAEndHost (RE AD0 HID0, HID0, 1500, 0, aa:aa:aa:aa:aa:aa);
host1 :: XIAEndHost (RE AD1 HID1, HID1, 11500, 1, aa:aa:aa:aa:aa:aa);
host2 :: XIAEndHost (RE AD2 HID2, HID2, 12500, 1, aa:aa:aa:aa:aa:aa);
host3 :: XIAEndHost (RE AD2 HID3, HID3, 12600, 1, aa:aa:aa:aa:aa:aa);

router0 :: XIARouter4Port(RE AD0 RHID0, AD0, RHID0, 0.0.0.0, 10700, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router1 :: XIARouter4Port(RE AD1 RHID1, AD1, RHID1, 0.0.0.0, 11700, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router2 :: XIARouter4Port(RE AD2 RHID2, AD2, RHID2, 0.0.0.0, 12700, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router3 :: XIARouter4Port(RE AD2 RHID3, AD2, RHID3, 0.0.0.0, 12800, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);

controller0 :: XIAController(RE AD0 CHID0, AD0, CHID0, 0.0.0.0, 10900, aa:aa:aa:aa:aa:aa);
controller1 :: XIAController(RE AD1 CHID1, AD1, CHID1, 0.0.0.0, 11900, aa:aa:aa:aa:aa:aa);
controller2 :: XIAController(RE AD2 CHID2, AD2, CHID2, 0.0.0.0, 12900, aa:aa:aa:aa:aa:aa);

// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// host0 :: nameserver

// Controller to router connections 
controller0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller0;

controller1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller1;

controller2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router3;
router3[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller2;

// Router to router connections
router0[1] -> LinkUnqueue(0.005, 1 GB/s) ->[3]router1;
router1[3] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

router1[1] -> LinkUnqueue(0.005, 1 GB/s) ->[3]router2;
router2[3] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router1;

router2[1] -> LinkUnqueue(0.005, 1 GB/s) ->[3]router3;
router3[3] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router2;

// Router to host connections
router0[2] -> LinkUnqueue(0.005, 1 GB/s) ->[0]host0;
host0[0] -> LinkUnqueue(0.005, 1 GB/s) ->[2]router0;

router1[2] -> LinkUnqueue(0.005, 1 GB/s) ->[0]host1;
host1[0] -> LinkUnqueue(0.005, 1 GB/s) ->[2]router1;

router2[2] -> LinkUnqueue(0.005, 1 GB/s) ->[0]host2;
host2[0] -> LinkUnqueue(0.005, 1 GB/s) ->[2]router2;

router3[2] -> LinkUnqueue(0.005, 1 GB/s) ->[0]host3;
host3[0] -> LinkUnqueue(0.005, 1 GB/s) ->[2]router3;

// Unused connections
router0[3] -> Idle;
Idle -> [3]router0;

router2[0] -> Idle;
Idle -> [0]router2;

router3[1] -> Idle;
Idle -> [1]router3;

ControlSocket(tcp, 7777);
