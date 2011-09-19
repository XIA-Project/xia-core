require(library xia_router_template.click); 
require(library xia_address.click); 
require(library xia_vm_common.click); 

router :: RouteEngine(RE AD0 RHID0);

to_tap0 :: Queue() -> ToDevice(tap0);
to_eth0 :: Queue() -> ToDevice(eth0);

FromDevice(eth0)
-> c0 :: Classifier(12/9999) -> Strip(14) -> MarkXIAHeader() -> [0]router

FromDevice(tap0, PROMISC true)
-> c1 :: Classifier(12/9999) -> Strip(14) -> MarkXIAHeader() -> [0]router

Idle -> [1]router;

router[0]
-> sw :: PaintSwitch;

sw[0]
-> XIAPrint("to_other")
-> EtherEncap(0x9999, RHID0, OTHER) -> to_eth0;

sw[1]
-> XIAPrint("to_guest")
-> EtherEncap(0x9999, RHID0, GUEST) -> to_tap0;

router[1]
-> XIAPrint("no_rpc") 
-> Discard;

router[2]
-> XIAPrint("no_cache") 
-> Discard;

Script(write router/proc/rt_AD/rt.add AD0 4);
Script(write router/proc/rt_AD/rt.add - 0);
Script(write router/proc/rt_HID/rt.add RHID0 4);
Script(write router/proc/rt_HID/rt.add HID0 1);

