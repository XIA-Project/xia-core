require(library xia_router_template.click); 
require(library xia_address.click); 

router :: RouteEngine(RE AD0 RHID1);

to_tap0 :: Queue() -> ToDevice(tap0);
to_eth0 :: Queue() -> ToDevice(eth0);

FromDevice(eth0, PROMISC true)
-> c0 :: Classifier(12/9999, -) -> Strip(14) -> MarkXIAHeader() -> [0]router
c0[1] -> Discard;

FromDevice(tap0, PROMISC true)
-> c1 :: Classifier(12/9999, -) -> Strip(14) -> MarkXIAHeader() -> [0]router
c1[1] -> Discard;

Idle -> [1]router;

// topology
// AD1:HID1 --+-- AD0:RHID0 --- HID0
//            +-- AD0:RHID1 --- HID0

router[0]
-> sw :: PaintSwitch;

router[1]
-> XIAPrint("no_rpc") 
-> Discard;

router[2]
-> XIAPrint("no_cache") 
-> Discard;

sw[0]
-> XIAPrint("to_AD1")
-> EtherEncap(0x9999, 00:0f:b5:9a:8b:2f, 00:0f:b5:3f:54:e3) -> to_eth0;

sw[1]
-> XIAPrint("to_hostB") 
-> EtherEncap(0x9999, 00:0f:b5:9a:8b:2f, 00:0f:b5:3f:54:6d) -> to_eth0;

sw[2]
-> XIAPrint("to_guest") 
-> EtherEncap(0x9999, 00:0f:b5:9a:8b:2f, 00:11:22:33:44:55) -> to_tap0;

Script(write router/proc/rt_AD/rt.add AD0 4);
Script(write router/proc/rt_AD/rt.add AD1 0);
Script(write router/proc/rt_HID/rt.add RHID0 1);
Script(write router/proc/rt_HID/rt.add RHID1 4);
Script(write router/proc/rt_HID/rt.add HID0 2);

