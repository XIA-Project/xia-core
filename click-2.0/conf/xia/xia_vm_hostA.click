require(library xia_router_template.click);
require(library xia_address.click);
require(library xia_vm_common.click);

router :: RouteEngine(RE AD0 RHID0);

from_eth0 :: FromDevice(eth0);
//from_eth0 :: Socket(TCP, 0.0.0.0, 7777);
from_tap0 :: FromDevice(tap0, PROMISC true);

to_eth0 :: Queue() -> ToDevice(eth0);
//to_eth0 :: Queue() -> from_eth0;
to_tap0 :: Queue() -> ToDevice(tap0);

from_eth0
-> c0 :: Classifier(12/9999) -> Strip(14) -> MarkXIAHeader() -> [0]router

from_tap0
-> c1 :: Classifier(12/9999) -> Strip(14) -> MarkXIAHeader() -> [0]router

Idle -> [1]router;

router[0]
-> sw :: PaintSwitch;

sw[0]
-> XIAPrint("xia_vm_hostA:to_other")
-> EtherEncap(0x9999, RHID0, OTHER) -> to_eth0;

sw[1]
-> XIAPrint("xia_vm_hostA:to_guest")
-> EtherEncap(0x9999, RHID0, GUEST) -> to_tap0;

router[1]
-> XIAPrint("xia_vm_hostA:no_rpc")
-> Discard;

router[2]
-> XIAPrint("xia_vm_hostA:no_cache")
-> Discard;

Script(write router/proc/rt_AD/rt.add AD0 4);
Script(write router/proc/rt_AD/rt.add - 0);
Script(write router/proc/rt_HID/rt.add RHID0 4);
Script(write router/proc/rt_HID/rt.add HID0 1);

