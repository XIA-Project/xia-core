require(library xia_router_template.click); 
require(library xia_address.click); 

router :: RouteEngine(RE HID0);

to_eth0 :: Queue() -> ToDevice(eth0);

FromDevice(eth0, PROMISC true)
-> c0 :: Classifier(12/9999, -) -> Strip(14) -> MarkXIAHeader() -> [0]router
c0[1] -> Discard;

router[0]
-> XIAPrint("to_outside")
-> EtherEncap(0x9999, 00:11:22:33:44:55, 66:77:88:99:aa:bb) -> to_eth0;

router[1]
-> XIAPingServer(RE AD0 RHID0, RE HID0)
-> [1]router;

router[2]
-> XIAPrint("no_cache") 
-> Discard;

Script(write router/proc/rt_AD/rt.add - 0);
Script(write router/proc/rt_HID/rt.add HID0 4);

