require(library ../../click/conf/xia_router_lib.click);
require(library xia_address.click);

// host0 :: nameserver

// host instantiation
host0 :: XIAEndHost (RE AD_INIT HID_NAMESERVER, HID_NAMESERVER, 1500, 0, 02:7f:45:93:4d:fc);


FromDevice(eth1, METHOD LINUX) -> [0]host0[0] -> ToDevice(eth1);


	ControlSocket(tcp, 7777);

