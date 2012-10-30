
require(library ../xia_mq_template.click);
require(library ../xia_address.click);

define($AD_RT_SIZE 351611);
define($AD_RANDOMIZE_MAX_CYCLE $AD_RT_SIZE);
define($PAYLOAD_SIZE 30);

rgen_fb2_0(HID2, xge0, xge2);
rgen_fb2_0(HID3, xge1, xge3);
rgen_fb2_0(HID4, xge2, xge0);
rgen_fb2_0(HID5, xge3, xge1);
