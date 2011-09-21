require(library xia_router_template.click);
require(library xia_address.click);
require(library xia_vm_common.click);

XIAPingUpdate(RE AD0 RHID1, RE AD0 RHID1 HID0, RE AD0 RHID1 HID0)
-> RatedUnqueue(1)
-> EtherEncap(0x9999, RHID1, GUEST)
-> Queue()
-> ToDevice(tap0)
-> XIAPrint()
-> Discard;

//FromDevice(tap0, PROMISC true)
//-> AggregateCounter(COUNT_STOP 1)
//-> XIAPrint()
//-> Discard;

