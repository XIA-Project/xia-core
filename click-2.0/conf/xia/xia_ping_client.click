require(library xia_router_template.click); 
require(library xia_address.click); 

router :: RouteEngine(RE HID1);

to_eth0 :: Queue() -> ToDevice(eth0);

FromDevice(eth0, PROMISC true)
-> c0 :: Classifier(12/9999, -) -> Strip(14) -> MarkXIAHeader() -> [0]router
c0[1] -> Discard;

router[0]
-> XIAPrint("to_outside")
-> EtherEncap(0x9999, 00:11:22:33:44:55, 00:0f:b5:3f:54:6d) -> to_eth0;

router[1]
-> XIAPingClient(RE AD1, RE HID1, RE AD0 RHID0 HID0)
-> [1]router;

router[2]
-> XIAPrint("no_cache") 
-> Discard;

Script(write router/proc/rt_AD/rt.add - 0);
Script(write router/proc/rt_HID/rt.add HID1 4);

