require(library xia_router_template.click);
require(library xia_address.click);
require(library xia_vm_common.click);

router :: RouteEngine(RE AD0 RHID0 HID0);

from_eth0 :: FromDevice(eth0, PROMISC true);

to_eth0 :: Queue() -> ToDevice(eth0);

from_eth0
-> c0 :: Classifier(12/9999) -> Strip(14) -> MarkXIAHeader() -> [0]router

router[0]
-> sw :: PaintSwitch;

sw[0]
-> XIAPrint("xia_vm_ping_server:to_other")
-> EtherEncap(0x9999, GUEST, OTHER) -> to_eth0;

router[1]
-> XIAPingResponder(RE AD0 RHID0 HID0)
-> XIAPrint("xia_vm_ping_server:resp")
-> [1]router;

router[2]
-> XIAPrint("xia_vm_ping_server:no_cache")
-> Discard;

Script(write router/proc/rt_AD/rt.add - 0);
Script(write router/proc/rt_HID/rt.add RHID0 4);
Script(write router/proc/rt_HID/rt.add RHID1 4);
Script(write router/proc/rt_HID/rt.add HID0 4);
Script(write router/proc/rt_HID/rt.add - 0);

