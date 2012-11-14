require(library xia_router_lib.click);
require(library xia_address.click);

// host & router instantiation
host0 :: XIAEndHost (RE AD0 HID0, HID0, fake0, 172.0.0.2, 172.0.0.1, 11:11:11:11:11:11, 0, aa:aa:aa:aa:aa:aa);
host1 :: XIAEndHost (RE AD1 HID1, HID1, fake1, 192.0.0.2, 192.0.0.1, 21:11:11:11:11:11, 1, aa:aa:aa:aa:aa:aa);

router0 :: XIARouter2Port(RE AD0 RHID0, AD0, RHID0, 0.0.0.0, fake2, 180.0.0.2, 180.0.0.1, 31:11:11:11:11:11, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, 0);
router1 :: XIARouter2Port(RE AD1 RHID1, AD1, RHID1, 0.0.0.0, fake3, 181.0.0.2, 181.0.0.1, 41:11:11:11:11:11, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, 0);

// Counter instantiation
host0_final_0::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
host0_next_0::XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -)
host1_final_0::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
host1_next_0::XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -)
router0_final_0::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
router0_next_0::XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -)
router0_final_1::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
router0_next_1::XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -)
router1_final_0::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
router1_next_0::XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -)
router1_final_1::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
router1_next_1::XIAXIDTypeCounter(next AD, next HID, next SID, next CID, next IP, -)

// interconnection -- host - ad
host0[0] -> host0_final_0 -> host0_next_0 -> LinkUnqueue(0.005, 1 GB/s) -> RandomSample(1.0) -> [0]router0;
router0[0] -> router0_final_0 -> router0_next_0 -> LinkUnqueue(0.005, 1 GB/s) -> [0]host0;

host1[0] -> host1_final_0 -> host1_next_0 -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] -> router1_final_0 -> router1_next_0 -> LinkUnqueue(0.005, 1 GB/s) ->[0]host1;

// interconnection -- ad - ad
router0[1] -> router0_final_1 -> router0_next_1 ->  LinkUnqueue(0.005, 1 GB/s) ->[1]router1;
router1[1] -> router1_final_1 -> router1_next_1 ->  LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

ControlSocket(tcp, 7777);
