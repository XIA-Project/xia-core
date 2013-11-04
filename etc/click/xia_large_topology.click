require(library xia_router_lib.click);
require(library xia_address.click);

// host & router instantiation
host0 :: XIAEndHost (RE AD0 HID0, HID0, 1500, 0, aa:aa:aa:aa:aa:aa);
host1 :: XIAEndHost (RE AD1 HID1, HID1, 1600, 1, aa:aa:aa:aa:aa:aa);
host2 :: XIAEndHost (RE AD2 HID2, HID2, 1700, 2, aa:aa:aa:aa:aa:aa);
host31 :: XIAEndHost (RE AD3 HID31, HID31, 1800, 3, aa:aa:aa:aa:aa:aa);
host32 :: XIAEndHost (RE AD3 HID32, HID32, 1900, 4, aa:aa:aa:aa:aa:aa);
host33 :: XIAEndHost (RE AD3 HID33, HID33, 2000, 5, aa:aa:aa:aa:aa:aa);

router0 :: XIARouter4Port(RE AD0 RHID0, AD0, RHID0, 0.0.0.0, 2100, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router1 :: XIARouter2Port(RE AD1 RHID1, AD1, RHID1, 0.0.0.0, 2200, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router2 :: XIARouter2Port(RE AD2 RHID2, AD2, RHID2, 0.0.0.0, 2300, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router3 :: XIARouter4Port(RE AD3 RHID3, AD3, RHID3, 0.0.0.0, 2400, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);


// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// router1 :: nameserver

// AD0 connections
host0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

router0[1] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router0;

router0[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router2;
router2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]router0;

router0[3] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router3;
router3[0] -> LinkUnqueue(0.005, 1 GB/s) -> [3]router0;


// AD1 connections
host1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;
router1[1] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host1;

// AD2 connections
host2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router2;
router2[1] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host2;

// AD3 connections
host31[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router3;
router3[1] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host31;

host32[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]router3;
router3[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host32;

host33[0] -> LinkUnqueue(0.005, 1 GB/s) -> [3]router3;
router3[3] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host33;




ControlSocket(tcp, 7777);
