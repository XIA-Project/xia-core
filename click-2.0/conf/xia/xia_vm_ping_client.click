require(library xia_router_template.click);
require(library xia_address.click);
require(library xia_vm_common.click);

router :: RouteEngine(RE AD1 HID1);

//from_eth0 :: FromDevice(eth0, PROMISC true);
from_eth0 :: FromDevice(br0, PROMISC true); // if client runs on RHID0

//to_eth0 :: Queue() -> ToDevice(eth0);
to_eth0 :: Queue() -> ToDevice(br0); // if client runs on RHID0

from_eth0
-> c0 :: Classifier(12/9999) -> Strip(14) -> MarkXIAHeader() -> [0]router;

Idle -> [1]router;

router[0]
-> sw :: PaintSwitch;

sw[0]
-> XIAPrint("xia_vm_ping_client:to_rhid0")
-> EtherEncap(0x9999, CLIENT, GUEST_AT_RHID0) -> to_eth0;

sw[1]
-> XIAPrint("xia_vm_ping_client:to_rhid1")
-> EtherEncap(0x9999, CLIENT, GUEST_AT_RHID1) -> to_eth0;

router[1]
-> XIAPingSource(RE AD1 HID1, RE AD0 RHID0 HID0, INTERVAL 1)
-> XIAPrint("xia_vm_ping_client:gen")
-> [0]router;

router[2]
-> XIAPrint("xia_vm_ping_client:no_cache")
-> Discard;

Script(write router/proc/rt_AD/rt.add AD0 4);
Script(write router/proc/rt_AD/rt.add AD1 4);
Script(write router/proc/rt_HID/rt.add RHID0 0);
Script(write router/proc/rt_HID/rt.add RHID1 1);
Script(write router/proc/rt_HID/rt.add HID1 4);

