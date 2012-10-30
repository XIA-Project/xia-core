#!/usr/local/sbin/click-install -uct12

require(library xia_router_template.click); 
require(library xia_address.click); 

router0 :: Router4PortDummyCacheNoQueue(RE AD0 RHID0);
toh ::Discard; 
c_eth2 :: Classifier(23/99, -);
c_eth3 :: Classifier(23/99, -);
c_eth4 :: Classifier(23/99, -);
c_eth5 :: Classifier(23/99, -);

pd_eth2_0:: MQPollDevice(eth2, QUEUE 0, BURST 32, PROMISC true) -> Paint(0, 17) -> c_eth2; 
pd_eth2_1:: MQPollDevice(eth2, QUEUE 1, BURST 32, PROMISC true) -> Paint(0, 17) -> c_eth2; 
pd_eth2_2:: MQPollDevice(eth2, QUEUE 2, BURST 32, PROMISC true) -> Paint(0, 17) -> c_eth2; 
pd_eth2_3:: MQPollDevice(eth2, QUEUE 3, BURST 32, PROMISC true) -> Paint(0, 17) -> c_eth2; 
pd_eth2_4:: MQPollDevice(eth2, QUEUE 4, BURST 32, PROMISC true) -> Paint(1, 17 ) -> c_eth2; 
pd_eth2_5:: MQPollDevice(eth2, QUEUE 5, BURST 32, PROMISC true) -> Paint(1, 17 ) -> c_eth2; 
pd_eth2_6:: MQPollDevice(eth2, QUEUE 6, BURST 32, PROMISC true) -> Paint(1, 17 ) -> c_eth2; 
pd_eth2_7:: MQPollDevice(eth2, QUEUE 7, BURST 32, PROMISC true) -> Paint(1, 17 ) -> c_eth2; 
pd_eth2_8:: MQPollDevice(eth2, QUEUE 8, BURST 32, PROMISC true) -> Paint(2, 17) -> c_eth2; 
pd_eth2_9:: MQPollDevice(eth2, QUEUE 9, BURST 32, PROMISC true) -> Paint(2, 17) -> c_eth2; 
pd_eth2_10:: MQPollDevice(eth2, QUEUE 10, BURST 32, PROMISC true)-> Paint(2, 17 )  -> c_eth2; 
pd_eth2_11:: MQPollDevice(eth2, QUEUE 11, BURST 32, PROMISC true)-> Paint(2, 17 )  -> c_eth2; 


pd_eth3_0:: MQPollDevice(eth3, QUEUE 0, BURST 32, PROMISC true)   -> Paint(3, 17 ) -> c_eth3; 
pd_eth3_1:: MQPollDevice(eth3, QUEUE 1, BURST 32, PROMISC true)   -> Paint(3, 17 ) -> c_eth3; 
pd_eth3_2:: MQPollDevice(eth3, QUEUE 2, BURST 32, PROMISC true)   -> Paint(3, 17 ) -> c_eth3; 
pd_eth3_3:: MQPollDevice(eth3, QUEUE 3, BURST 32, PROMISC true)   -> Paint(3, 17 ) -> c_eth3; 
pd_eth3_4:: MQPollDevice(eth3, QUEUE 4, BURST 32, PROMISC true)   -> Paint(4, 17 ) -> c_eth3; 
pd_eth3_5:: MQPollDevice(eth3, QUEUE 5, BURST 32, PROMISC true)   -> Paint(4, 17 ) -> c_eth3; 
pd_eth3_6:: MQPollDevice(eth3, QUEUE 6, BURST 32, PROMISC true)   -> Paint(4, 17 ) -> c_eth3; 
pd_eth3_7:: MQPollDevice(eth3, QUEUE 7, BURST 32, PROMISC true)   -> Paint(4, 17 ) -> c_eth3; 
pd_eth3_8:: MQPollDevice(eth3, QUEUE 8, BURST 32, PROMISC true)   -> Paint(5, 17 ) -> c_eth3; 
pd_eth3_9:: MQPollDevice(eth3, QUEUE 9, BURST 32, PROMISC true)   -> Paint(5, 17 ) -> c_eth3; 
pd_eth3_10:: MQPollDevice(eth3, QUEUE 10, BURST 32, PROMISC true) -> Paint(5, 17 ) -> c_eth3; 
pd_eth3_11:: MQPollDevice(eth3, QUEUE 11, BURST 32, PROMISC true) -> Paint(5, 17 ) -> c_eth3; 


pd_eth4_0:: MQPollDevice(eth4, QUEUE 0, BURST 32, PROMISC true)  -> Paint(6, 17 )-> c_eth4; 
pd_eth4_1:: MQPollDevice(eth4, QUEUE 1, BURST 32, PROMISC true)  -> Paint(6, 17 )-> c_eth4; 
pd_eth4_2:: MQPollDevice(eth4, QUEUE 2, BURST 32, PROMISC true)  -> Paint(6, 17 )-> c_eth4; 
pd_eth4_3:: MQPollDevice(eth4, QUEUE 3, BURST 32, PROMISC true)  -> Paint(6, 17 )-> c_eth4; 
pd_eth4_4:: MQPollDevice(eth4, QUEUE 4, BURST 32, PROMISC true)  -> Paint(7, 17 )-> c_eth4; 
pd_eth4_5:: MQPollDevice(eth4, QUEUE 5, BURST 32, PROMISC true)  -> Paint(7, 17 )-> c_eth4; 
pd_eth4_6:: MQPollDevice(eth4, QUEUE 6, BURST 32, PROMISC true)  -> Paint(7, 17 )-> c_eth4; 
pd_eth4_7:: MQPollDevice(eth4, QUEUE 7, BURST 32, PROMISC true)  -> Paint(7, 17 )-> c_eth4; 
pd_eth4_8:: MQPollDevice(eth4, QUEUE 8, BURST 32, PROMISC true)  -> Paint(8, 17 )-> c_eth4; 
pd_eth4_9:: MQPollDevice(eth4, QUEUE 9, BURST 32, PROMISC true)  -> Paint(8, 17 )-> c_eth4; 
pd_eth4_10:: MQPollDevice(eth4, QUEUE 10, BURST 32, PROMISC true)-> Paint(8, 17 )-> c_eth4; 
pd_eth4_11:: MQPollDevice(eth4, QUEUE 11, BURST 32, PROMISC true)-> Paint(8, 17 )-> c_eth4; 


pd_eth5_0:: MQPollDevice(eth5, QUEUE 0, BURST 32, PROMISC true)  -> Paint(9, 17 )-> c_eth5; 
pd_eth5_1:: MQPollDevice(eth5, QUEUE 1, BURST 32, PROMISC true)  -> Paint(9, 17 )-> c_eth5; 
pd_eth5_2:: MQPollDevice(eth5, QUEUE 2, BURST 32, PROMISC true)  -> Paint(9, 17 )-> c_eth5; 
pd_eth5_3:: MQPollDevice(eth5, QUEUE 3, BURST 32, PROMISC true)  -> Paint(9, 17 )-> c_eth5; 
pd_eth5_4:: MQPollDevice(eth5, QUEUE 4, BURST 32, PROMISC true)  -> Paint(10, 17 )-> c_eth5; 
pd_eth5_5:: MQPollDevice(eth5, QUEUE 5, BURST 32, PROMISC true)  -> Paint(10, 17 )-> c_eth5; 
pd_eth5_6:: MQPollDevice(eth5, QUEUE 6, BURST 32, PROMISC true)  -> Paint(10, 17 )-> c_eth5; 
pd_eth5_7:: MQPollDevice(eth5, QUEUE 7, BURST 32, PROMISC true)  -> Paint(10, 17 )-> c_eth5; 
pd_eth5_8:: MQPollDevice(eth5, QUEUE 8, BURST 32, PROMISC true)  -> Paint(11, 17 )-> c_eth5; 
pd_eth5_9:: MQPollDevice(eth5, QUEUE 9, BURST 32, PROMISC true)  -> Paint(11, 17 )-> c_eth5; 
pd_eth5_10:: MQPollDevice(eth5, QUEUE 10, BURST 32, PROMISC true)-> Paint(11, 17 ) -> c_eth5; 
pd_eth5_11:: MQPollDevice(eth5, QUEUE 11, BURST 32, PROMISC true)-> Paint(11, 17 ) -> c_eth5; 

c_eth2[0] -> Strip(34) -> MarkXIAHeader() ->[0]router0; // XIA packet  
c_eth3[0] -> Strip(34) -> MarkXIAHeader() -> [1]router0; // XIA packet  
c_eth4[0] -> Strip(34) -> MarkXIAHeader() -> [2]router0; // XIA packet  
c_eth5[0] -> Strip(34) -> MarkXIAHeader() -> [3]router0; // XIA packet  
c_eth2[1] -> toh;
c_eth3[1] -> toh;
c_eth4[1] -> toh;
c_eth5[1] -> toh;

router0[0] 
//-> XIAPrint() 
-> Unstrip(20)
-> encap0::EtherEncap(0x0800, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77)
-> eth2_ps::PaintSwitch(17)


router0[1] 
//-> XIAPrint() 
-> Unstrip(20)
-> encap1::EtherEncap(0x0800, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77)
-> eth3_ps::PaintSwitch(17)

router0[2] 
//-> XIAPrint() 
-> Unstrip(20)
-> encap2::EtherEncap(0x0800, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77) 
-> eth4_ps::PaintSwitch(17)

router0[3] 
//-> XIAPrint() 
-> Unstrip(20)
-> encap3::EtherEncap(0x0800, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77)
-> eth5_ps::PaintSwitch(17)

 eth2_ps[0] -> tod_eth2_0 :: MQPushToDevice(eth2, QUEUE 0, BURST 32) 
 eth2_ps[1] -> tod_eth2_1 :: MQPushToDevice(eth2, QUEUE 1, BURST 32) 
 eth2_ps[2] -> tod_eth2_2 :: MQPushToDevice(eth2, QUEUE 2, BURST 32) 
 eth2_ps[3] -> tod_eth2_3 :: MQPushToDevice(eth2, QUEUE 3, BURST 32) 
 eth2_ps[4] -> tod_eth2_4 :: MQPushToDevice(eth2, QUEUE 4, BURST 32) 
 eth2_ps[5] -> tod_eth2_5 :: MQPushToDevice(eth2, QUEUE 5, BURST 32) 
 eth2_ps[6] -> tod_eth2_6 :: MQPushToDevice(eth2, QUEUE 6, BURST 32) 
 eth2_ps[7] -> tod_eth2_7 :: MQPushToDevice(eth2, QUEUE 7, BURST 32) 
 eth2_ps[8] -> tod_eth2_8 :: MQPushToDevice(eth2, QUEUE 8, BURST 32) 
 eth2_ps[9] -> tod_eth2_9 :: MQPushToDevice(eth2, QUEUE 9, BURST 32) 
 eth2_ps[10]-> tod_eth2_10 :: MQPushToDevice(eth2, QUEUE 10, BURST 32) 
 eth2_ps[11]-> tod_eth2_11 :: MQPushToDevice(eth2, QUEUE 11, BURST 32) 

 eth3_ps[0]  -> tod_eth3_0 :: MQPushToDevice(eth3, QUEUE 0, BURST 32) 
 eth3_ps[1]  -> tod_eth3_1 :: MQPushToDevice(eth3, QUEUE 1, BURST 32) 
 eth3_ps[2]  -> tod_eth3_2 :: MQPushToDevice(eth3, QUEUE 2, BURST 32) 
 eth3_ps[3]  -> tod_eth3_3 :: MQPushToDevice(eth3, QUEUE 3, BURST 32) 
 eth3_ps[4]  -> tod_eth3_4 :: MQPushToDevice(eth3, QUEUE 4, BURST 32) 
 eth3_ps[5]  -> tod_eth3_5 :: MQPushToDevice(eth3, QUEUE 5, BURST 32) 
 eth3_ps[6]  -> tod_eth3_6 :: MQPushToDevice(eth3, QUEUE 6, BURST 32) 
 eth3_ps[7]  -> tod_eth3_7 :: MQPushToDevice(eth3, QUEUE 7, BURST 32) 
 eth3_ps[8]  -> tod_eth3_8 :: MQPushToDevice(eth3, QUEUE 8, BURST 32) 
 eth3_ps[9]  -> tod_eth3_9 :: MQPushToDevice(eth3, QUEUE 9, BURST 32) 
 eth3_ps[10] -> tod_eth3_10 :: MQPushToDevice(eth3, QUEUE 10, BURST 32) 
 eth3_ps[11] -> tod_eth3_11 :: MQPushToDevice(eth3, QUEUE 11, BURST 32) 

 eth4_ps[0] -> tod_eth4_0 :: MQPushToDevice(eth4, QUEUE 0, BURST 32) 
 eth4_ps[1] -> tod_eth4_1 :: MQPushToDevice(eth4, QUEUE 1, BURST 32) 
 eth4_ps[2] -> tod_eth4_2 :: MQPushToDevice(eth4, QUEUE 2, BURST 32) 
 eth4_ps[3] -> tod_eth4_3 :: MQPushToDevice(eth4, QUEUE 3, BURST 32) 
 eth4_ps[4] -> tod_eth4_4 :: MQPushToDevice(eth4, QUEUE 4, BURST 32) 
 eth4_ps[5] -> tod_eth4_5 :: MQPushToDevice(eth4, QUEUE 5, BURST 32) 
 eth4_ps[6] -> tod_eth4_6 :: MQPushToDevice(eth4, QUEUE 6, BURST 32) 
 eth4_ps[7] -> tod_eth4_7 :: MQPushToDevice(eth4, QUEUE 7, BURST 32) 
 eth4_ps[8] -> tod_eth4_8 :: MQPushToDevice(eth4, QUEUE 8, BURST 32) 
 eth4_ps[9] -> tod_eth4_9 :: MQPushToDevice(eth4, QUEUE 9, BURST 32) 
 eth4_ps[10]-> tod_eth4_10 :: MQPushToDevice(eth4, QUEUE 10, BURST 32) 
 eth4_ps[11]-> tod_eth4_11 :: MQPushToDevice(eth4, QUEUE 11, BURST 32) 

 eth5_ps[0]  -> tod_eth5_0 :: MQPushToDevice(eth5, QUEUE 0, BURST 32) 
 eth5_ps[1]  -> tod_eth5_1 :: MQPushToDevice(eth5, QUEUE 1, BURST 32) 
 eth5_ps[2]  -> tod_eth5_2 :: MQPushToDevice(eth5, QUEUE 2, BURST 32) 
 eth5_ps[3]  -> tod_eth5_3 :: MQPushToDevice(eth5, QUEUE 3, BURST 32) 
 eth5_ps[4]  -> tod_eth5_4 :: MQPushToDevice(eth5, QUEUE 4, BURST 32) 
 eth5_ps[5]  -> tod_eth5_5 :: MQPushToDevice(eth5, QUEUE 5, BURST 32) 
 eth5_ps[6]  -> tod_eth5_6 :: MQPushToDevice(eth5, QUEUE 6, BURST 32) 
 eth5_ps[7]  -> tod_eth5_7 :: MQPushToDevice(eth5, QUEUE 7, BURST 32) 
 eth5_ps[8]  -> tod_eth5_8 :: MQPushToDevice(eth5, QUEUE 8, BURST 32) 
 eth5_ps[9]  -> tod_eth5_9 :: MQPushToDevice(eth5, QUEUE 9, BURST 32) 
 eth5_ps[10] -> tod_eth5_10 :: MQPushToDevice(eth5, QUEUE 10, BURST 32) 
 eth5_ps[11] -> tod_eth5_11 :: MQPushToDevice(eth5, QUEUE 11, BURST 32) 


StaticThreadSched(pd_eth2_0  0, pd_eth3_0  3,    pd_eth4_0 6,   pd_eth5_0 9 );
StaticThreadSched(pd_eth2_1  0, pd_eth3_1  3,    pd_eth4_1 6,   pd_eth5_1 9 );
StaticThreadSched(pd_eth2_2  0, pd_eth3_2  3,    pd_eth4_2 6,   pd_eth5_2 9 );
StaticThreadSched(pd_eth2_3  0, pd_eth3_3  3,    pd_eth4_3 6,   pd_eth5_3 9 );
StaticThreadSched(pd_eth2_4  1, pd_eth3_4  4,    pd_eth4_4 7,   pd_eth5_4 10 );
StaticThreadSched(pd_eth2_5  1, pd_eth3_5  4,    pd_eth4_5 7,   pd_eth5_5 10 );
StaticThreadSched(pd_eth2_6  1, pd_eth3_6  4,    pd_eth4_6 7,   pd_eth5_6 10 );
StaticThreadSched(pd_eth2_7  1, pd_eth3_7  4,    pd_eth4_7 7,   pd_eth5_7 10 );
StaticThreadSched(pd_eth2_8  2, pd_eth3_8  5,    pd_eth4_8 8,   pd_eth5_8 11 );
StaticThreadSched(pd_eth2_9  2, pd_eth3_9  5,    pd_eth4_9 8,   pd_eth5_9 11 );
StaticThreadSched(pd_eth2_10 2, pd_eth3_10 5,    pd_eth4_10 8,  pd_eth5_10 11 );
StaticThreadSched(pd_eth2_11 2, pd_eth3_11 5,    pd_eth4_11 8,  pd_eth5_11 11 );

Script(write router0/n/proc/rt_HID/rt.add HID2 0);  // eth2's address 
Script(write router0/n/proc/rt_HID/rt.add HID3 1);  // eth3's address 
Script(write router0/n/proc/rt_HID/rt.add HID4 2);  // eth4's address 
Script(write router0/n/proc/rt_HID/rt.add HID5 3);  // eth5's address 
