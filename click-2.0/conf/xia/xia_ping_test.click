require(library xia_router_template.click);
require(library xia_address.click);

src :: XIAPingSource(RE HID1, RE HID0, INTERVAL 1);
dst :: XIAPingResponder(RE HID0);
upd :: XIAPingUpdate(RE HID2, RE HID0, RE HID3);

src
-> XIAPrint("src->dst")
-> dst
-> XIAPrint("dst->src")
-> src;

upd -> RatedUnqueue(1) -> dst;

