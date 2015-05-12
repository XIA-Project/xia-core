require(library xia_router_lib.click);
require(library xia_address.click);

// Controller instantiation
Controller :: XIAController (RE AD0 CHID0, AD0, CHID0, 0.0.0.0, 2000, aa:aa:aa:aa:aa:aa);


// router0 instantiation
router0 :: XIARouter4Port(RE AD0 RHID0_0, AD0, RHID0_0, 0.0.0.0, 1600, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);

// Controller to router0 connections 
controller0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller0;


// router1 instantiation
router1 :: XIARouter4Port(RE AD0 RHID0_1, AD0, RHID0_1, 0.0.0.0, 1700, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);

// router0 to router1 connections 
router1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router0;
router0[1] -> LinkUnqueue(0.005, 1 GB/s) -> router1[0];


// router2 instantiation
router2 :: XIARouter4Port(RE AD0 RHID0_2, AD0, RHID0_2, 0.0.0.0, 1800, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);

// router0 to router2 connections 
router2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]router0;
router0[2] -> LinkUnqueue(0.005, 1 GB/s) -> router2[0];

// router3 instantiation
router3 :: XIARouter4Port(RE AD0 RHID0_3, AD0, RHID0_3, 0.0.0.0, 1900, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);

// router0 to router1 connections 
router3[0] -> LinkUnqueue(0.005, 1 GB/s) -> [3]router0;
router0[3] -> LinkUnqueue(0.005, 1 GB/s) -> router3[0];

// host0
host0 :: XIAEndHost (RE AD0 HID0, HID0, 1500, 0, aa:aa:aa:aa:aa:aa);
// host0 to router1
host0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;
router1[1] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;



//Open ports

// avoid echo, block traffic from localhost
allow :: RadixIPLookup(0.0.0.0/0 0);;
deny :: RadixIPLookup(127.0.0.1 0);;
deny -> allow -> deny;;// meaningless, only for syntax 

sockR1P2::Socket("UDP", 0.0.0.0, 5001, CLIENT false, DENY deny) -> [2]router1[2] -> sockR1P2;
sockR1P3::Socket("UDP", 0.0.0.0, 5002, CLIENT false, DENY deny) -> [3]router1[3] -> sockR1P3;

sockR2P1::Socket("UDP", 0.0.0.0, 5003, CLIENT false, DENY deny) -> [1]router2[1] -> sockR2P1;
sockR2P2::Socket("UDP", 0.0.0.0, 5004, CLIENT false, DENY deny) -> [2]router2[2] -> sockR2P2;
sockR2P3::Socket("UDP", 0.0.0.0, 5005, CLIENT false, DENY deny) -> [3]router2[3] -> sockR2P3;


sockR3P1::Socket("UDP", 0.0.0.0, 5006, CLIENT false, DENY deny) -> [1]router3[1] -> sockR3P1;
sockR3P2::Socket("UDP", 0.0.0.0, 5007, CLIENT false, DENY deny) -> [2]router3[2] -> sockR3P2;
sockR3P3::Socket("UDP", 0.0.0.0, 5008, CLIENT false, DENY deny) -> [3]router3[3] -> sockR3P3;



    ControlSocket(tcp, 7777);
