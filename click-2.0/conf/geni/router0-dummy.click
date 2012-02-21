require(library xia_two_port_four_port_router.click); 

// router instantiation
router0 :: RouterDummyCache(RE AD0 RHID0, AD0, RHID0);
//router0 :: Router(RE AD0 RHID0, AD0, RHID0);
//router0 :: Router4Port(RE AD0 RHID0);
//router0 :: Router4PortDummyCache(RE AD0 RHID0, AD0, RHID0); // if router does not understand CID pricipal


c0 :: Classifier(12/9999);
c1 :: Classifier(12/9999);

todevice0 :: ToDevice(eth1);
todevice1 :: ToDevice(eth2);

FromDevice(eth1) -> c0;
c0[0] -> Strip(14) -> MarkXIAHeader() -> [0]router0; // XIA packet 
FromDevice(eth2) -> c1;
c1[0] -> Strip(14) -> MarkXIAHeader() -> [1]router0; // XIA packet 

//Idle -> [2]router0[2] -> Discard; 
//Idle -> [3]router0[3] -> Discard; 


router0[0]
//-> XIAPrint() 
-> EtherEncap(0x9999, 00:24:e8:30:2d:96, 00:1B:21:3A:D5:75) 
-> c::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -)
-> todevice0;

router0[1]
//-> XIAPrint() 
-> EtherEncap(0x9999, 00:1b:21:3a:d7:50, 00:24:E8:30:AD:59) -> todevice1; 

ControlSocket(tcp, 7777);


//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed




//Script(write router0/n/proc/rt_AD/rt.add AD0 4);     // no default route for CID; consider other path
//Script(write router0/n/proc/rt_AD/rt.add AD1 1);     // no default route for CID; consider other path
//Script(write router0/n/proc/rt_AD/rt.add - 5);     // no default route for CID; consider other path

//Script(write router0/n/proc/rt_HID/rt.add HID0 0);     // no default route for CID; consider other path
//Script(write router0/n/proc/rt_HID/rt.add RHID0 4);     // no default route for CID; consider other path
//Script(write router0/n/proc/rt_HID/rt.add - 5);     // no default route for CID; consider other path

//Script(write router0/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write router0/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path





