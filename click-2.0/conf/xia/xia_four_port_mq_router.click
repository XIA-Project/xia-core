#!/usr/local/sbin/click-install -uct12

require(library xia_router_template.click); 
require(library xia_address.click); 

// router instantiation
//router0 :: Router4PortDummyCache(RE AD0 RHID0, AD0, RHID0);
router0 :: Router4PortDummyCache(RE AD0 RHID0);
toh :: ToHost; 
c_eth2 :: Classifier(12/9999, -);
c_eth3 :: Classifier(12/9999, -);
c_eth4 :: Classifier(12/9999, -);
c_eth5 :: Classifier(12/9999, -);

pd_eth2_0:: MQPollDevice(eth2, QUEUE 0, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_1:: MQPollDevice(eth2, QUEUE 1, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_2:: MQPollDevice(eth2, QUEUE 2, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_3:: MQPollDevice(eth2, QUEUE 3, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_4:: MQPollDevice(eth2, QUEUE 4, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_5:: MQPollDevice(eth2, QUEUE 5, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_6:: MQPollDevice(eth2, QUEUE 6, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_7:: MQPollDevice(eth2, QUEUE 7, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_8:: MQPollDevice(eth2, QUEUE 8, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_9:: MQPollDevice(eth2, QUEUE 9, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_10:: MQPollDevice(eth2, QUEUE 10, BURST 32, PROMISC true) -> c_eth2; 
pd_eth2_11:: MQPollDevice(eth2, QUEUE 11, BURST 32, PROMISC true) -> c_eth2; 


pd_eth3_0:: MQPollDevice(eth3, QUEUE 0, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_1:: MQPollDevice(eth3, QUEUE 1, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_2:: MQPollDevice(eth3, QUEUE 2, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_3:: MQPollDevice(eth3, QUEUE 3, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_4:: MQPollDevice(eth3, QUEUE 4, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_5:: MQPollDevice(eth3, QUEUE 5, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_6:: MQPollDevice(eth3, QUEUE 6, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_7:: MQPollDevice(eth3, QUEUE 7, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_8:: MQPollDevice(eth3, QUEUE 8, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_9:: MQPollDevice(eth3, QUEUE 9, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_10:: MQPollDevice(eth3, QUEUE 10, BURST 32, PROMISC true) -> c_eth3; 
pd_eth3_11:: MQPollDevice(eth3, QUEUE 11, BURST 32, PROMISC true) -> c_eth3; 


pd_eth4_0:: MQPollDevice(eth4, QUEUE 0, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_1:: MQPollDevice(eth4, QUEUE 1, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_2:: MQPollDevice(eth4, QUEUE 2, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_3:: MQPollDevice(eth4, QUEUE 3, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_4:: MQPollDevice(eth4, QUEUE 4, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_5:: MQPollDevice(eth4, QUEUE 5, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_6:: MQPollDevice(eth4, QUEUE 6, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_7:: MQPollDevice(eth4, QUEUE 7, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_8:: MQPollDevice(eth4, QUEUE 8, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_9:: MQPollDevice(eth4, QUEUE 9, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_10:: MQPollDevice(eth4, QUEUE 10, BURST 32, PROMISC true) -> c_eth4; 
pd_eth4_11:: MQPollDevice(eth4, QUEUE 11, BURST 32, PROMISC true) -> c_eth4; 


pd_eth5_0:: MQPollDevice(eth5, QUEUE 0, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_1:: MQPollDevice(eth5, QUEUE 1, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_2:: MQPollDevice(eth5, QUEUE 2, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_3:: MQPollDevice(eth5, QUEUE 3, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_4:: MQPollDevice(eth5, QUEUE 4, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_5:: MQPollDevice(eth5, QUEUE 5, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_6:: MQPollDevice(eth5, QUEUE 6, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_7:: MQPollDevice(eth5, QUEUE 7, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_8:: MQPollDevice(eth5, QUEUE 8, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_9:: MQPollDevice(eth5, QUEUE 9, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_10:: MQPollDevice(eth5, QUEUE 10, BURST 32, PROMISC true) -> c_eth5; 
pd_eth5_11:: MQPollDevice(eth5, QUEUE 11, BURST 32, PROMISC true) -> c_eth5; 

c_eth2[0] -> Strip(14) -> MarkXIAHeader() -> [0]router0; // XIA packet  
c_eth3[0] -> Strip(14) -> MarkXIAHeader() -> [1]router0; // XIA packet  
c_eth4[0] -> Strip(14) -> MarkXIAHeader() -> [2]router0; // XIA packet  
c_eth5[0] -> Strip(14) -> MarkXIAHeader() -> [3]router0; // XIA packet  
c_eth2[1] -> toh;
c_eth3[1] -> toh;
c_eth4[1] -> toh;
c_eth5[1] -> toh;
router0[0] 
//-> XIAPrint() 
-> encap0::EtherEncap(0x9999, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); 
router0[1] 
//-> XIAPrint() 
-> encap1::EtherEncap(0x9999, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); 
router0[2] 
//-> XIAPrint() 
-> encap2::EtherEncap(0x9999, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); 
router0[3] 
//-> XIAPrint() 
-> encap3::EtherEncap(0x9999, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); 

 encap0 -> tod_eth2_0 :: MQToDevice(eth2, QUEUE 0, BURST 32) 
 encap0 -> tod_eth2_1 :: MQToDevice(eth2, QUEUE 1, BURST 32) 
 encap0 -> tod_eth2_2 :: MQToDevice(eth2, QUEUE 2, BURST 32) 
 encap0 -> tod_eth2_3 :: MQToDevice(eth2, QUEUE 3, BURST 32) 
 encap0 -> tod_eth2_4 :: MQToDevice(eth2, QUEUE 4, BURST 32) 
 encap0 -> tod_eth2_5 :: MQToDevice(eth2, QUEUE 5, BURST 32) 
 encap0 -> tod_eth2_6 :: MQToDevice(eth2, QUEUE 6, BURST 32) 
 encap0 -> tod_eth2_7 :: MQToDevice(eth2, QUEUE 7, BURST 32) 
 encap0 -> tod_eth2_8 :: MQToDevice(eth2, QUEUE 8, BURST 32) 
 encap0 -> tod_eth2_9 :: MQToDevice(eth2, QUEUE 9, BURST 32) 
 encap0 -> tod_eth2_10 :: MQToDevice(eth2, QUEUE 10, BURST 32) 
 encap0 -> tod_eth2_11 :: MQToDevice(eth2, QUEUE 11, BURST 32) 

 encap1 -> tod_eth3_0 :: MQToDevice(eth3, QUEUE 0, BURST 32) 
 encap1 -> tod_eth3_1 :: MQToDevice(eth3, QUEUE 1, BURST 32) 
 encap1 -> tod_eth3_2 :: MQToDevice(eth3, QUEUE 2, BURST 32) 
 encap1 -> tod_eth3_3 :: MQToDevice(eth3, QUEUE 3, BURST 32) 
 encap1 -> tod_eth3_4 :: MQToDevice(eth3, QUEUE 4, BURST 32) 
 encap1 -> tod_eth3_5 :: MQToDevice(eth3, QUEUE 5, BURST 32) 
 encap1 -> tod_eth3_6 :: MQToDevice(eth3, QUEUE 6, BURST 32) 
 encap1 -> tod_eth3_7 :: MQToDevice(eth3, QUEUE 7, BURST 32) 
 encap1 -> tod_eth3_8 :: MQToDevice(eth3, QUEUE 8, BURST 32) 
 encap1 -> tod_eth3_9 :: MQToDevice(eth3, QUEUE 9, BURST 32) 
 encap1 -> tod_eth3_10 :: MQToDevice(eth3, QUEUE 10, BURST 32) 
 encap1 -> tod_eth3_11 :: MQToDevice(eth3, QUEUE 11, BURST 32) 

 encap2 -> tod_eth4_0 :: MQToDevice(eth4, QUEUE 0, BURST 32) 
 encap2 -> tod_eth4_1 :: MQToDevice(eth4, QUEUE 1, BURST 32) 
 encap2 -> tod_eth4_2 :: MQToDevice(eth4, QUEUE 2, BURST 32) 
 encap2 -> tod_eth4_3 :: MQToDevice(eth4, QUEUE 3, BURST 32) 
 encap2 -> tod_eth4_4 :: MQToDevice(eth4, QUEUE 4, BURST 32) 
 encap2 -> tod_eth4_5 :: MQToDevice(eth4, QUEUE 5, BURST 32) 
 encap2 -> tod_eth4_6 :: MQToDevice(eth4, QUEUE 6, BURST 32) 
 encap2 -> tod_eth4_7 :: MQToDevice(eth4, QUEUE 7, BURST 32) 
 encap2 -> tod_eth4_8 :: MQToDevice(eth4, QUEUE 8, BURST 32) 
 encap2 -> tod_eth4_9 :: MQToDevice(eth4, QUEUE 9, BURST 32) 
 encap2 -> tod_eth4_10 :: MQToDevice(eth4, QUEUE 10, BURST 32) 
 encap2 -> tod_eth4_11 :: MQToDevice(eth4, QUEUE 11, BURST 32) 

 encap3 -> tod_eth5_0 :: MQToDevice(eth5, QUEUE 0, BURST 32) 
 encap3 -> tod_eth5_1 :: MQToDevice(eth5, QUEUE 1, BURST 32) 
 encap3 -> tod_eth5_2 :: MQToDevice(eth5, QUEUE 2, BURST 32) 
 encap3 -> tod_eth5_3 :: MQToDevice(eth5, QUEUE 3, BURST 32) 
 encap3 -> tod_eth5_4 :: MQToDevice(eth5, QUEUE 4, BURST 32) 
 encap3 -> tod_eth5_5 :: MQToDevice(eth5, QUEUE 5, BURST 32) 
 encap3 -> tod_eth5_6 :: MQToDevice(eth5, QUEUE 6, BURST 32) 
 encap3 -> tod_eth5_7 :: MQToDevice(eth5, QUEUE 7, BURST 32) 
 encap3 -> tod_eth5_8 :: MQToDevice(eth5, QUEUE 8, BURST 32) 
 encap3 -> tod_eth5_9 :: MQToDevice(eth5, QUEUE 9, BURST 32) 
 encap3 -> tod_eth5_10 :: MQToDevice(eth5, QUEUE 10, BURST 32) 
 encap3 -> tod_eth5_11 :: MQToDevice(eth5, QUEUE 11, BURST 32) 


StaticThreadSched(pd_eth2_0 0, tod_eth2_0 0, pd_eth3_0 0, tod_eth3_0 0, pd_eth4_0 0, tod_eth4_0 0, pd_eth5_0 0, tod_eth5_0 0 );
StaticThreadSched(pd_eth2_1 1, tod_eth2_1 1, pd_eth3_1 1, tod_eth3_1 1, pd_eth4_1 1, tod_eth4_1 1, pd_eth5_1 1, tod_eth5_1 1 );
StaticThreadSched(pd_eth2_2 2, tod_eth2_2 2, pd_eth3_2 2, tod_eth3_2 2, pd_eth4_2 2, tod_eth4_2 2, pd_eth5_2 2, tod_eth5_2 2 );
StaticThreadSched(pd_eth2_3 3, tod_eth2_3 3, pd_eth3_3 3, tod_eth3_3 3, pd_eth4_3 3, tod_eth4_3 3, pd_eth5_3 3, tod_eth5_3 3 );
StaticThreadSched(pd_eth2_4 4, tod_eth2_4 4, pd_eth3_4 4, tod_eth3_4 4, pd_eth4_4 4, tod_eth4_4 4, pd_eth5_4 4, tod_eth5_4 4 );
StaticThreadSched(pd_eth2_5 5, tod_eth2_5 5, pd_eth3_5 5, tod_eth3_5 5, pd_eth4_5 5, tod_eth4_5 5, pd_eth5_5 5, tod_eth5_5 5 );
StaticThreadSched(pd_eth2_6 6, tod_eth2_6 6, pd_eth3_6 6, tod_eth3_6 6, pd_eth4_6 6, tod_eth4_6 6, pd_eth5_6 6, tod_eth5_6 6 );
StaticThreadSched(pd_eth2_7 7, tod_eth2_7 7, pd_eth3_7 7, tod_eth3_7 7, pd_eth4_7 7, tod_eth4_7 7, pd_eth5_7 7, tod_eth5_7 7 );
StaticThreadSched(pd_eth2_8 8, tod_eth2_8 8, pd_eth3_8 8, tod_eth3_8 8, pd_eth4_8 8, tod_eth4_8 8, pd_eth5_8 8, tod_eth5_8 8 );
StaticThreadSched(pd_eth2_9 9, tod_eth2_9 9, pd_eth3_9 9, tod_eth3_9 9, pd_eth4_9 9, tod_eth4_9 9, pd_eth5_9 9, tod_eth5_9 9 );
StaticThreadSched(pd_eth2_10 10, tod_eth2_10 10, pd_eth3_10 10, tod_eth3_10 10, pd_eth4_10 10, tod_eth4_10 10, pd_eth5_10 10, tod_eth5_10 10 );
StaticThreadSched(pd_eth2_11 11, tod_eth2_11 11, pd_eth3_11 11, tod_eth3_11 11, pd_eth4_11 11, tod_eth4_11 11, pd_eth5_11 11, tod_eth5_11 11 );

 Script(write router0/n/proc/rt_HID/rt.add HID2 0);  // eth2's address 
 Script(write router0/n/proc/rt_HID/rt.add HID3 1);  // eth3's address 
 Script(write router0/n/proc/rt_HID/rt.add HID4 2);  // eth4's address 
 Script(write router0/n/proc/rt_HID/rt.add HID5 3);  // eth5's address 
