#!/usr/local/sbin/click-install -uct12
define($AD_RT_SIZE 351611);
define($AD_RANDOMIZE_MAX_CYCLE $AD_RT_SIZE);

require(library xia_mq_template.click);
require(library xia_address.click);

rgen_nofb_0(HID2, eth2, eth4);
rgen_nofb_0(HID3, eth3, eth5);
rgen_nofb_0(HID4, eth4, eth2);
rgen_nofb_0(HID5, eth5, eth3);
