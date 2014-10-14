require(library ../../click/conf/xia_router_lib.click);
require(library xia_address.click);

// host & router instantiation
host0 :: XIAEndHost (RE AD_INIT HID_NAMESERVER, HID_NAMESERVER, 1500, 0, 02:7f:45:93:4d:fc);

router0 :: XIARouter4Port(RE AD_NAMESERVER HID_ROUTER0, AD_NAMESERVER, HID_ROUTER0, 0.0.0.0, 1600, 02:6c:d9:70:b1:d8, 02:0d:2f:56:79:5f, 00:00:00:00:00:00, 00:00:00:00:00:00);
router1 :: XIARouter4Port(RE AD_SERVERS HID_ROUTER1, AD_SERVERS, HID_ROUTER1, 0.0.0.0, 1700, 02:f4:7a:fa:66:a7, 02:7f:c1:96:19:af, 02:ba:ff:c5:54:d4, 00:00:00:00:00:00);

server0 :: XIAEndHost (RE AD_INIT HID_SERVER0, HID_SERVER0, 1800, 0, 02:c4:a7:8a:b4:2f);
server1 :: XIAEndHost (RE AD_INIT HID_SERVER1, HID_SERVER1, 1900, 0, 02:26:f0:ad:e5:74);

// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// host0 :: nameserver

host0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;
router0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

router0[1] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router0;

router1[1] -> LinkUnqueue(0.005, 1 GB/s) -> [0]server0;
server0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;

router1[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]server1;
server1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]router1;

Idle -> [2]router0[2] -> Idle
Idle -> [3]router0[3] -> Idle
Idle -> [3]router1[3] -> Idle

ControlSocket(tcp, 7777);
