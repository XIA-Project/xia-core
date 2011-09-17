require(library ../xia_router_template.click); 
require(library ../xia_address.click); 

define($AD_RT_SIZE 351611);

// router instantiation
//router0 :: Router4PortDummyCache(RE AD0 RHID0, AD0, RHID0);
router0 :: Router4PortDummyCache(RE ADSELF RHID0);
//toh :: ToHost; 
toh :: Counter -> XIAPrint(toh)-> Discard; 
c_xge0 :: Classifier(23/99, -);
c_xge1 :: Classifier(23/99, -);
c_xge2 :: Classifier(23/99, -);
c_xge3 :: Classifier(23/99, -);

pd_xge0_0:: MQPollDevice(xge0, QUEUE 0, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_1:: MQPollDevice(xge0, QUEUE 1, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_2:: MQPollDevice(xge0, QUEUE 2, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_3:: MQPollDevice(xge0, QUEUE 3, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_4:: MQPollDevice(xge0, QUEUE 4, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_5:: MQPollDevice(xge0, QUEUE 5, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_6:: MQPollDevice(xge0, QUEUE 6, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_7:: MQPollDevice(xge0, QUEUE 7, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_8:: MQPollDevice(xge0, QUEUE 8, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_9:: MQPollDevice(xge0, QUEUE 9, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_10:: MQPollDevice(xge0, QUEUE 10, BURST 32, PROMISC true) -> c_xge0; 
pd_xge0_11:: MQPollDevice(xge0, QUEUE 11, BURST 32, PROMISC true) -> c_xge0; 


pd_xge1_0:: MQPollDevice(xge1, QUEUE 0, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_1:: MQPollDevice(xge1, QUEUE 1, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_2:: MQPollDevice(xge1, QUEUE 2, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_3:: MQPollDevice(xge1, QUEUE 3, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_4:: MQPollDevice(xge1, QUEUE 4, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_5:: MQPollDevice(xge1, QUEUE 5, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_6:: MQPollDevice(xge1, QUEUE 6, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_7:: MQPollDevice(xge1, QUEUE 7, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_8:: MQPollDevice(xge1, QUEUE 8, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_9:: MQPollDevice(xge1, QUEUE 9, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_10:: MQPollDevice(xge1, QUEUE 10, BURST 32, PROMISC true) -> c_xge1; 
pd_xge1_11:: MQPollDevice(xge1, QUEUE 11, BURST 32, PROMISC true) -> c_xge1; 


pd_xge2_0:: MQPollDevice(xge2, QUEUE 0, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_1:: MQPollDevice(xge2, QUEUE 1, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_2:: MQPollDevice(xge2, QUEUE 2, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_3:: MQPollDevice(xge2, QUEUE 3, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_4:: MQPollDevice(xge2, QUEUE 4, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_5:: MQPollDevice(xge2, QUEUE 5, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_6:: MQPollDevice(xge2, QUEUE 6, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_7:: MQPollDevice(xge2, QUEUE 7, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_8:: MQPollDevice(xge2, QUEUE 8, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_9:: MQPollDevice(xge2, QUEUE 9, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_10:: MQPollDevice(xge2, QUEUE 10, BURST 32, PROMISC true) -> c_xge2; 
pd_xge2_11:: MQPollDevice(xge2, QUEUE 11, BURST 32, PROMISC true) -> c_xge2; 


pd_xge3_0:: MQPollDevice(xge3, QUEUE 0, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_1:: MQPollDevice(xge3, QUEUE 1, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_2:: MQPollDevice(xge3, QUEUE 2, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_3:: MQPollDevice(xge3, QUEUE 3, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_4:: MQPollDevice(xge3, QUEUE 4, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_5:: MQPollDevice(xge3, QUEUE 5, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_6:: MQPollDevice(xge3, QUEUE 6, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_7:: MQPollDevice(xge3, QUEUE 7, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_8:: MQPollDevice(xge3, QUEUE 8, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_9:: MQPollDevice(xge3, QUEUE 9, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_10:: MQPollDevice(xge3, QUEUE 10, BURST 32, PROMISC true) -> c_xge3; 
pd_xge3_11:: MQPollDevice(xge3, QUEUE 11, BURST 32, PROMISC true) -> c_xge3; 

c_xge0[0] -> Strip(34) -> MarkXIAHeader() -> [0]router0; // XIA packet  
c_xge1[0] -> Strip(34) -> MarkXIAHeader() -> [1]router0; // XIA packet  
c_xge2[0] -> Strip(34) -> MarkXIAHeader() -> [2]router0; // XIA packet  
c_xge3[0] -> Strip(34) -> MarkXIAHeader() -> [3]router0; // XIA packet  
c_xge0[1] -> toh;
c_xge1[1] -> toh;
c_xge2[1] -> toh;
c_xge3[1] -> toh;

router0[0] 
//-> XIAPrint(input) 
-> Unstrip(20)
-> encap0::EtherEncap(0x0800, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); 
router0[1] 
//-> XIAPrint() 
-> Unstrip(20)
-> encap1::EtherEncap(0x0800, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); 
router0[2] 
//-> XIAPrint() 
-> Unstrip(20)
-> encap2::EtherEncap(0x0800, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); 
router0[3] 
//-> XIAPrint() 
-> Unstrip(20)
-> encap3::EtherEncap(0x0800, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); 

 encap0 -> tod_xge0_0 :: MQToDevice(xge0, QUEUE 0, BURST 32) 
 encap0 -> tod_xge0_1 :: MQToDevice(xge0, QUEUE 1, BURST 32) 
 encap0 -> tod_xge0_2 :: MQToDevice(xge0, QUEUE 2, BURST 32) 
 encap0 -> tod_xge0_3 :: MQToDevice(xge0, QUEUE 3, BURST 32) 
 encap0 -> tod_xge0_4 :: MQToDevice(xge0, QUEUE 4, BURST 32) 
 encap0 -> tod_xge0_5 :: MQToDevice(xge0, QUEUE 5, BURST 32) 
 encap0 -> tod_xge0_6 :: MQToDevice(xge0, QUEUE 6, BURST 32) 
 encap0 -> tod_xge0_7 :: MQToDevice(xge0, QUEUE 7, BURST 32) 
 encap0 -> tod_xge0_8 :: MQToDevice(xge0, QUEUE 8, BURST 32) 
 encap0 -> tod_xge0_9 :: MQToDevice(xge0, QUEUE 9, BURST 32) 
 encap0 -> tod_xge0_10 :: MQToDevice(xge0, QUEUE 10, BURST 32) 
 encap0 -> tod_xge0_11 :: MQToDevice(xge0, QUEUE 11, BURST 32) 

 encap1 -> tod_xge1_0 :: MQToDevice(xge1, QUEUE 0, BURST 32) 
 encap1 -> tod_xge1_1 :: MQToDevice(xge1, QUEUE 1, BURST 32) 
 encap1 -> tod_xge1_2 :: MQToDevice(xge1, QUEUE 2, BURST 32) 
 encap1 -> tod_xge1_3 :: MQToDevice(xge1, QUEUE 3, BURST 32) 
 encap1 -> tod_xge1_4 :: MQToDevice(xge1, QUEUE 4, BURST 32) 
 encap1 -> tod_xge1_5 :: MQToDevice(xge1, QUEUE 5, BURST 32) 
 encap1 -> tod_xge1_6 :: MQToDevice(xge1, QUEUE 6, BURST 32) 
 encap1 -> tod_xge1_7 :: MQToDevice(xge1, QUEUE 7, BURST 32) 
 encap1 -> tod_xge1_8 :: MQToDevice(xge1, QUEUE 8, BURST 32) 
 encap1 -> tod_xge1_9 :: MQToDevice(xge1, QUEUE 9, BURST 32) 
 encap1 -> tod_xge1_10 :: MQToDevice(xge1, QUEUE 10, BURST 32) 
 encap1 -> tod_xge1_11 :: MQToDevice(xge1, QUEUE 11, BURST 32) 

 encap2 -> tod_xge2_0 :: MQToDevice(xge2, QUEUE 0, BURST 32) 
 encap2 -> tod_xge2_1 :: MQToDevice(xge2, QUEUE 1, BURST 32) 
 encap2 -> tod_xge2_2 :: MQToDevice(xge2, QUEUE 2, BURST 32) 
 encap2 -> tod_xge2_3 :: MQToDevice(xge2, QUEUE 3, BURST 32) 
 encap2 -> tod_xge2_4 :: MQToDevice(xge2, QUEUE 4, BURST 32) 
 encap2 -> tod_xge2_5 :: MQToDevice(xge2, QUEUE 5, BURST 32) 
 encap2 -> tod_xge2_6 :: MQToDevice(xge2, QUEUE 6, BURST 32) 
 encap2 -> tod_xge2_7 :: MQToDevice(xge2, QUEUE 7, BURST 32) 
 encap2 -> tod_xge2_8 :: MQToDevice(xge2, QUEUE 8, BURST 32) 
 encap2 -> tod_xge2_9 :: MQToDevice(xge2, QUEUE 9, BURST 32) 
 encap2 -> tod_xge2_10 :: MQToDevice(xge2, QUEUE 10, BURST 32) 
 encap2 -> tod_xge2_11 :: MQToDevice(xge2, QUEUE 11, BURST 32) 

 encap3 -> tod_xge3_0 :: MQToDevice(xge3, QUEUE 0, BURST 32) 
 encap3 -> tod_xge3_1 :: MQToDevice(xge3, QUEUE 1, BURST 32) 
 encap3 -> tod_xge3_2 :: MQToDevice(xge3, QUEUE 2, BURST 32) 
 encap3 -> tod_xge3_3 :: MQToDevice(xge3, QUEUE 3, BURST 32) 
 encap3 -> tod_xge3_4 :: MQToDevice(xge3, QUEUE 4, BURST 32) 
 encap3 -> tod_xge3_5 :: MQToDevice(xge3, QUEUE 5, BURST 32) 
 encap3 -> tod_xge3_6 :: MQToDevice(xge3, QUEUE 6, BURST 32) 
 encap3 -> tod_xge3_7 :: MQToDevice(xge3, QUEUE 7, BURST 32) 
 encap3 -> tod_xge3_8 :: MQToDevice(xge3, QUEUE 8, BURST 32) 
 encap3 -> tod_xge3_9 :: MQToDevice(xge3, QUEUE 9, BURST 32) 
 encap3 -> tod_xge3_10 :: MQToDevice(xge3, QUEUE 10, BURST 32) 
 encap3 -> tod_xge3_11 :: MQToDevice(xge3, QUEUE 11, BURST 32) 


StaticThreadSched(pd_xge0_0 0, tod_xge0_0 0, pd_xge1_0 0, tod_xge1_0 0, pd_xge2_0 0, tod_xge2_0 0, pd_xge3_0 0, tod_xge3_0 0 );
StaticThreadSched(pd_xge0_1 1, tod_xge0_1 1, pd_xge1_1 1, tod_xge1_1 1, pd_xge2_1 1, tod_xge2_1 1, pd_xge3_1 1, tod_xge3_1 1 );
StaticThreadSched(pd_xge0_2 2, tod_xge0_2 2, pd_xge1_2 2, tod_xge1_2 2, pd_xge2_2 2, tod_xge2_2 2, pd_xge3_2 2, tod_xge3_2 2 );
StaticThreadSched(pd_xge0_3 3, tod_xge0_3 3, pd_xge1_3 3, tod_xge1_3 3, pd_xge2_3 3, tod_xge2_3 3, pd_xge3_3 3, tod_xge3_3 3 );
StaticThreadSched(pd_xge0_4 4, tod_xge0_4 4, pd_xge1_4 4, tod_xge1_4 4, pd_xge2_4 4, tod_xge2_4 4, pd_xge3_4 4, tod_xge3_4 4 );
StaticThreadSched(pd_xge0_5 5, tod_xge0_5 5, pd_xge1_5 5, tod_xge1_5 5, pd_xge2_5 5, tod_xge2_5 5, pd_xge3_5 5, tod_xge3_5 5 );
StaticThreadSched(pd_xge0_6 6, tod_xge0_6 6, pd_xge1_6 6, tod_xge1_6 6, pd_xge2_6 6, tod_xge2_6 6, pd_xge3_6 6, tod_xge3_6 6 );
StaticThreadSched(pd_xge0_7 7, tod_xge0_7 7, pd_xge1_7 7, tod_xge1_7 7, pd_xge2_7 7, tod_xge2_7 7, pd_xge3_7 7, tod_xge3_7 7 );
StaticThreadSched(pd_xge0_8 8, tod_xge0_8 8, pd_xge1_8 8, tod_xge1_8 8, pd_xge2_8 8, tod_xge2_8 8, pd_xge3_8 8, tod_xge3_8 8 );
StaticThreadSched(pd_xge0_9 9, tod_xge0_9 9, pd_xge1_9 9, tod_xge1_9 9, pd_xge2_9 9, tod_xge2_9 9, pd_xge3_9 9, tod_xge3_9 9 );
StaticThreadSched(pd_xge0_10 10, tod_xge0_10 10, pd_xge1_10 10, tod_xge1_10 10, pd_xge2_10 10, tod_xge2_10 10, pd_xge3_10 10, tod_xge3_10 10 );
StaticThreadSched(pd_xge0_11 11, tod_xge0_11 11, pd_xge1_11 11, tod_xge1_11 11, pd_xge2_11 11, tod_xge2_11 11, pd_xge3_11 11, tod_xge3_11 11 );

Script(write router0/n/proc/rt_HID/rt.add HID2 0);  // xge0's address 
Script(write router0/n/proc/rt_HID/rt.add HID3 1);  // xge1's address 
Script(write router0/n/proc/rt_HID/rt.add HID4 2);  // xge2's address 
Script(write router0/n/proc/rt_HID/rt.add HID5 3);  // xge3's address 

Script(write router0/n/proc/rt_AD/rt.add AD0 0);  // for isolation experiment
Script(write router0/n/proc/rt_AD/rt.add AD1 1);  // 
Script(write router0/n/proc/rt_AD/rt.add AD2 2);  // 
Script(write router0/n/proc/rt_AD/rt.add AD3 3);  // 
Script(write router0/n/proc/rt_AD/rt.add AD4 0);  // for isolation experiment
Script(write router0/n/proc/rt_AD/rt.add AD5 1);  // 
Script(write router0/n/proc/rt_AD/rt.add AD6 2);  // 
Script(write router0/n/proc/rt_AD/rt.add AD7 3);  // 
Script(write router0/n/proc/rt_AD/rt.add AD8 0);  // for isolation experiment
Script(write router0/n/proc/rt_AD/rt.add AD9 1);  // 
Script(write router0/n/proc/rt_AD/rt.add AD10 2);  // 
Script(write router0/n/proc/rt_AD/rt.add AD11 3);  // 


Script(write router0/n/proc/rt_CID/rt.add - 5);   
Script(write router0/n/proc/rt_AD/rt.add - 5);   
Script(write router0/n/proc/rt_AD/rt.add SELF_AD 4);   
Script(write router0/n/proc/rt_SID/rt.add - 5);   
Script(write router0/n/proc/rt_HID/rt.add - 5);   

Script(write router0/n/proc/rt_AD/rt.generate AD $AD_RT_SIZE -4);
//Script(write router0/n/proc/rt_AD/rt.debug 100);
