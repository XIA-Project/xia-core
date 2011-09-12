#!/usr/local/sbin/click-install -uct12
require(library xia_mq_template.click);

define($AD_RT_SIZE 351611);
define($AD_RANDOMIZE_MAX_CYCLE $AD_RT_SIZE);

rgen_fb1_0(HID2, eth2, eth4);
rgen_fb1_0(HID3, eth3, eth5);
rgen_fb1_0(HID4, eth4, eth2);
rgen_fb1_0(HID5, eth5, eth3);

