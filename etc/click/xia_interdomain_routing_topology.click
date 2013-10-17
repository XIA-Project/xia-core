require(library xia_router_lib.click);
require(library xia_address.click);

// host & router instantiation
//host0 :: XIAEndHost (RE AD0 HID0, HID0, 1500, 0, aa:aa:aa:aa:aa:aa);
host1 :: XIAEndHost (RE AD0 HID1, HID1, 1500, 1, aa:aa:aa:aa:aa:aa);
host2 :: XIAEndHost (RE AD1 HID2, HID2, 2500, 1, aa:aa:aa:aa:aa:aa);

router0 :: XIARouter4Port(RE AD0 RHID0, AD0, RHID0, 0.0.0.0, 1700, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router1 :: XIARouter2Port(RE AD0 RHID1, AD0, RHID1, 0.0.0.0, 1800, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router2 :: XIARouter4Port(RE AD1 RHID2, AD1, RHID0, 0.0.0.0, 2700, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router3 :: XIARouter2Port(RE AD1 RHID3, AD1, RHID0, 0.0.0.0, 2800, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);

controller0 :: XIAController(RE AD0 CHID0, AD0, CHID0, 0.0.0.0, 1900, aa:aa:aa:aa:aa:aa);
controller1 :: XIAController(RE AD1 CHID0, AD1, CHID0, 0.0.0.0, 2900, aa:aa:aa:aa:aa:aa);

// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// host0 :: nameserver

// interconnection -- AD0
controller0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller0;

router0[1] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router1;
router1[1] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

router1[0] -> LinkUnqueue(0.005, 1 GB/s) ->[0]host1;
host1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;

// interconnection -- AD1
controller1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router2;
router2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller1;

router2[1] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router3;
router3[1] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router2;

router3[0] -> LinkUnqueue(0.005, 1 GB/s) ->[0]host2;
host2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router3;

// interconnection -- AD0 - AD1
router0[2] -> LinkUnqueue(0.005, 1 GB/s) ->[2]router2;
router2[2] -> LinkUnqueue(0.005, 1 GB/s) ->[2]router0;

ControlSocket(tcp, 7777);
