require(library ../../click/conf/xia_router_lib.click);
require(library xia_address.click);

// host instantiation
server0 :: XIAEndHost (RE AD_INIT HID_SERVER0, HID_SERVER0, 1500, 0, 02:c4:a7:8a:b4:2f);


FromDevice(eth1, METHOD LINUX) -> [0]server0[0] -> ToDevice(eth1);


	ControlSocket(tcp, 7777);

