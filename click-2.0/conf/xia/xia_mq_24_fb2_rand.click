#!/usr/local/sbin/click-install -uct12
define($AD_RT_SIZE 351611);
define($AD_RANDOMIZE_MAX_CYCLE $AD_RT_SIZE);

require(library xia_mq_template.click);
require(library xia_address.click);

rgen_fb2_0(HID2, CID0, CID1, HID4, eth2, eth4);
rgen_fb2_0(HID3, CID0, CID1, HID5, eth3, eth5);
rgen_fb2_0(HID4, CID0, CID1, HID2, eth4, eth2);
rgen_fb2_0(HID5, CID0, CID1, HID3, eth5, eth3);
