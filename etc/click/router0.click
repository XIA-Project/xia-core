require(library ../../click/conf/xia_router_lib.click);
require(library xia_address.click);

// router instantiation
router0 :: XIARouter4Port(RE AD_NAMESERVER HID_ROUTER0, AD_NAMESERVER, HID_ROUTER0, 0.0.0.0, 1500, 02:6c:d9:70:b1:d8, 02:0d:2f:56:79:5f, 00:00:00:00:00:00, 00:00:00:00:00:00);


FromDevice(eth1, METHOD LINUX) -> [0]router0[0] -> ToDevice(eth1)

FromDevice(eth2, METHOD LINUX) -> [1]router0[1] -> ToDevice(eth2)

Idle -> [2]router0[2] -> Discard;

Idle -> [3]router0[3] -> Discard;


	ControlSocket(tcp, 7777);
