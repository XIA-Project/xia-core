require(library xia_two_port_four_port_router.click); 


// host instantiation
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1);
//router1 :: Router4PortDummyCache(RE AD1 RHID1, AD1, RHID1); // if router does not understand CID pricipal

c0 :: Classifier(12/9999);

todevice0 :: ToDevice(eth3);

FromDevice(eth3, PROMISC true) -> c0;
c0[0] -> Strip(14) -> MarkXIAHeader() -> [0]host1; // XIA packet 



host1[0]
//-> XIAPrint() 
-> EtherEncap(0x9999, 00:1B:21:3A:D5:99, 00:1B:21:3A:0E:D0) 
//-> Print() 
-> todevice0;


//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed


//Script(write host1/n/proc/rt_AD/rt.add AD1 4);    
//Script(write host1/n/proc/rt_AD/rt.add - 0); 

//Script(write host1/n/proc/rt_HID/rt.add HID1 4);  
//Script(write host1/n/proc/rt_HID/rt.add - 0);      

//Script(write host1/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write host1/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path





