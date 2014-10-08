require(library ../../click/conf/xia_router_lib.click);
require(library xia_address.click);

// host instantiation
server1 :: XIAEndHost (RE AD_INIT HID_SERVER1, HID_SERVER1, 1500, 0, 02:26:f0:ad:e5:74);


FromDevice(eth1, METHOD LINUX) -> [0]server1[0] -> ToDevice(eth1);


	ControlSocket(tcp, 7777);

