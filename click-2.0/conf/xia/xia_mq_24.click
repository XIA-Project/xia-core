#!/usr/local/sbin/click-install -uct24
require(library xia_mq_template.click);

gen_nofb_0(HID2, HID4, eth2);
gen_nofb_1(HID3, HID5, eth3);
gen_nofb_0(HID4, HID2, eth4);
gen_nofb_1(HID5, HID3, eth5);

