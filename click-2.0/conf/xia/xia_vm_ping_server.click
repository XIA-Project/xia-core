require(library xia_router_template.click); 
require(library xia_address.click); 
require(library xia_vm_common.click); 

router :: RouteEngine(RE AD0 RHID0 HID0);

to_eth0 :: Queue() -> ToDevice(eth0);

FromDevice(eth0)
-> c0 :: Classifier(12/9999) -> Strip(14) -> MarkXIAHeader() -> [0]router

router[0]
-> sw :: PaintSwitch;

sw[0]
-> XIAPrint("to_host")
-> EtherEncap(0x9999, 00:11:22:33:44:55, 66:77:88:99:aa:bb) -> to_eth0;

router[1]
-> XIAPingResponder(RE AD0 RHID0 HID0)
-> [1]router;

router[2]
-> XIAPrint("no_cache") 
-> Discard;

Script(write router/proc/rt_AD/rt.add - 0);
Script(write router/proc/rt_HID/rt.add HID0 4);
Script(write router/proc/rt_HID/rt.add - 0);

