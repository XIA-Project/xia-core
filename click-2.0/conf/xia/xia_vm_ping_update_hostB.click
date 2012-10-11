require(library xia_router_template.click);
require(library xia_address.click);
require(library xia_vm_common.click);

XIAPingUpdate(RE AD0 RHID1, RE AD0 RHID1 HID0, RE AD0 RHID1 HID0)
//-> RatedUnqueue(1)
-> Unqueue()
-> Clone(COUNT 1)
-> Unqueue()
-> EtherEncap(0xC0DE, RHID0, RHID1)
-> Queue()
-> ToDevice(eth0)
-> Queue()
-> ToDevice(tap0)
-> XIAPrint()
-> AggregateCounter(COUNT_STOP 1)
-> Discard;

