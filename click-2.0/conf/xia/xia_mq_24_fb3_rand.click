#!/usr/local/sbin/click-install -uct12
require(library xia_mq_template.click);

rgen_fb3_0(HID2, CID0, CID1, CID2, HID4, eth2, eth4);
rgen_fb3_0(HID3, CID0, CID1, CID2, HID5, eth3, eth5);
rgen_fb3_0(HID4, CID0, CID1, CID2, HID2, eth4, eth2);
rgen_fb3_0(HID5, CID0, CID1, CID2, HID3, eth5, eth3);

