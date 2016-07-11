require(library xia_router_lib.click);
require(library xia_address.click);


// host & router instantiation
host0 :: XIAEndHost (1500, host0, 1501, 1502, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
host1 :: XIAEndHost (1600, host1, 1601, 1602, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);

router0 :: XIARouter4Port(1700, router0, 1701, 1702, 0.0.0.0, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router1 :: XIARouter4Port(1800, router1, 1801, 1802, 0.0.0.0, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);

// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// router0 :: nameserver

// interconnection -- host - ad
host0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

host1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] -> LinkUnqueue(0.005, 1 GB/s) ->[0]host1;

// interconnection -- ad - ad
router0[1] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router1;
router1[1] -> LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

ControlSocket(tcp, 7777);
log::XLog(VERBOSE 0, LEVEL 6);

Idle -> [2]router0[2] -> Discard;
Idle -> [3]router0[3] -> Discard;
Idle -> [2]router1[2] -> Discard;
Idle -> [3]router1[3] -> Discard;
