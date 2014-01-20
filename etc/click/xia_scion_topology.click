require(library xia_router_lib.click);
require(library xia_address.click);

controller0 :: XIAController(RE AD0 CHID0, AD0, CHID0, 0.0.0.0, 1500, aa:aa:aa:aa:aa:aa);
beacon0 :: XIASCIONBeaconServer(RE AD0 HID0, AD0, HID0, 0.0.0.0, 1600, aa:aa:aa:aa:aa:aa);

controller0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]beacon0;
beacon0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller0;

// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// controller0 :: nameserver

ControlSocket(tcp, 7777);
