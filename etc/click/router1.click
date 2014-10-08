require(library ../../click/conf/xia_router_lib.click);
require(library xia_address.click);

// router instantiation
router1 :: XIARouter4Port(RE AD_SERVERS HID_ROUTER1, AD_SERVERS, HID_ROUTER1, 0.0.0.0, 1500, 02:f4:7a:fa:66:a7, 02:7f:c1:96:19:af, 02:ba:ff:c5:54:d4, 00:00:00:00:00:00);


FromDevice(eth1, METHOD LINUX) -> [0]router1[0] -> ToDevice(eth1)

FromDevice(eth2, METHOD LINUX) -> [1]router1[1] -> ToDevice(eth2)

FromDevice(eth3, METHOD LINUX) -> [2]router1[2] -> ToDevice(eth3)

Idle -> [3]router1[3] -> Discard;


	ControlSocket(tcp, 7777);
