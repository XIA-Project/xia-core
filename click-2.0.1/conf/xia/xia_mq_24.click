#!/usr/local/sbin/click-install -uct12
require(library xia_mq_template.click);
require(library xia_address.click);

gen_nofb_0(HID2, HID4, eth2, eth4);
gen_nofb_0(HID3, HID5, eth3, eth5);
gen_nofb_0(HID4, HID2, eth4, eth2);
gen_nofb_0(HID5, HID3, eth5, eth3);
