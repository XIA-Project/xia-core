require(library xia_router_template.click);
require(library xia_address.click);


// router instantiation
router0 :: RouterDummyCache(RE AD0 RHID0, AD0, RHID0);
toh :: ToHost;

c0 :: Classifier(12/C0DE, -);
c1 :: Classifier(12/C0DE, -);


pd_eth3_0:: MQPollDevice(eth3, QUEUE 0, BURST 32, PROMISC true) -> c0; 
pd_eth3_1:: MQPollDevice(eth3, QUEUE 1, BURST 32, PROMISC true) -> c0; 
pd_eth3_2:: MQPollDevice(eth3, QUEUE 2, BURST 32, PROMISC true) -> c0; 
pd_eth3_3:: MQPollDevice(eth3, QUEUE 3, BURST 32, PROMISC true) -> c0; 
pd_eth3_4:: MQPollDevice(eth3, QUEUE 4, BURST 32, PROMISC true) -> c0; 
pd_eth3_5:: MQPollDevice(eth3, QUEUE 5, BURST 32, PROMISC true) -> c0; 

c0[0] -> Strip(14) -> MarkXIAHeader() -> [0]router0; // XIA packet 

pd_eth5_0:: MQPollDevice(eth5, QUEUE 0, BURST 32, PROMISC true) -> c1; 
pd_eth5_1:: MQPollDevice(eth5, QUEUE 1, BURST 32, PROMISC true) -> c1; 
pd_eth5_2:: MQPollDevice(eth5, QUEUE 2, BURST 32, PROMISC true) -> c1; 
pd_eth5_3:: MQPollDevice(eth5, QUEUE 3, BURST 32, PROMISC true) -> c1; 
pd_eth5_4:: MQPollDevice(eth5, QUEUE 4, BURST 32, PROMISC true) -> c1; 
pd_eth5_5:: MQPollDevice(eth5, QUEUE 5, BURST 32, PROMISC true) -> c1; 

c1[0] -> Strip(14) -> MarkXIAHeader() -> [1]router0; // XIA packet 

c0[1] -> toh;
c1[1] -> toh;

router0[0]
-> XIAPrint() 
-> encap0::EtherEncap(0xC0DE, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77);

encap0 -> tod_eth3_0 :: MQToDevice(eth3, QUEUE 0, BURST 32);
encap0 -> tod_eth3_1 :: MQToDevice(eth3, QUEUE 1, BURST 32);
encap0 -> tod_eth3_2 :: MQToDevice(eth3, QUEUE 2, BURST 32); 
encap0 -> tod_eth3_3 :: MQToDevice(eth3, QUEUE 3, BURST 32); 
encap0 -> tod_eth3_4 :: MQToDevice(eth3, QUEUE 4, BURST 32); 
encap0 -> tod_eth3_5 :: MQToDevice(eth3, QUEUE 5, BURST 32); 

router0[1]
-> XIAPrint() 
-> encap1::EtherEncap(0xC0DE, 00:15:17:51:d3:d5, 00:1B:21:01:39:95); 
encap1 -> tod_eth5_0 :: MQToDevice(eth5, QUEUE 0, BURST 32) 
encap1 -> tod_eth5_1 :: MQToDevice(eth5, QUEUE 1, BURST 32) 
encap1 -> tod_eth5_2 :: MQToDevice(eth5, QUEUE 2, BURST 32) 
encap1 -> tod_eth5_3 :: MQToDevice(eth5, QUEUE 3, BURST 32) 
encap1 -> tod_eth5_4 :: MQToDevice(eth5, QUEUE 4, BURST 32) 
encap1 -> tod_eth5_5 :: MQToDevice(eth5, QUEUE 5, BURST 32) 


StaticThreadSched(pd_eth3_0 0, tod_eth3_0 0, tod_eth5_0 0, pd_eth5_0 0);
StaticThreadSched(pd_eth3_1 1, tod_eth3_1 1, tod_eth5_1 1, pd_eth5_1 1);
StaticThreadSched(pd_eth3_2 2, tod_eth3_2 2, tod_eth5_2 2, pd_eth5_2 2);
StaticThreadSched(pd_eth3_3 3, tod_eth3_3 3, tod_eth5_3 3, pd_eth5_3 3);
StaticThreadSched(pd_eth3_4 4, tod_eth3_4 4, tod_eth5_4 4, pd_eth5_4 4);
StaticThreadSched(pd_eth3_5 5, tod_eth3_5 5, tod_eth5_5 5, pd_eth5_5 5);

Script(write router0/n/proc/rt_HID/rt.add HID0 0);     
Script(write router0/n/proc/rt_HID/rt.add HID1 1);    

