require(library xia_router_template.click);
require(library xia_address.click);
require(library xia_vm_common.click);

to_tap0 :: Queue() -> ToDevice(tap0);

XIAPingUpdate(RE HID2, RE AD0 RHID1 HID0, RE AD0 RHID1 HID0)
-> Unqueue()
-> Clone(1)
-> Unqueue()
-> EtherEncap(0x9999, RHID1, GUEST)
-> XIAPrint()
-> to_tap0
-> AggregateCounter(COUNT_STOP 1)
-> Discard;

