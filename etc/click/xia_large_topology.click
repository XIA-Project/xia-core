require(library xia_router_lib.click);
require(library xia_address.click);

log::XLog(VERBOSE 0, LEVEL 6);

// host & router instantiation
host0 :: XIAEndHost (1500, 0, aa:aa:aa:aa:aa:aa);
host1 :: XIAEndHost (1600, 1, aa:aa:aa:aa:aa:aa);
host2 :: XIAEndHost (1700, 2, aa:aa:aa:aa:aa:aa);
host31 :: XIAEndHost (1800, 3, aa:aa:aa:aa:aa:aa);
host32 :: XIAEndHost (1900, 4, aa:aa:aa:aa:aa:aa);
host33 :: XIAEndHost (2000, 5, aa:aa:aa:aa:aa:aa);

router0 :: XIARouter4Port(2100, 0.0.0.0, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router1 :: XIARouter2Port(2200, 0.0.0.0, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router2 :: XIARouter2Port(2300, 0.0.0.0, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router3 :: XIARouter4Port(2400, 0.0.0.0, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);


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
