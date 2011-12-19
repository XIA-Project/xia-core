require(library ../../../xia_router_template_xtransport.click);
require(library ../../../xia_address.click);


// router instantiation
router0 :: Router(RE AD0 RHID0, AD0, RHID0);
//router0 :: Router4Port(RE AD0 RHID0);
//router0 :: Router4PortDummyCache(RE AD0 RHID0, AD0, RHID0); // if router does not understand CID pricipal


c0 :: Classifier(12/9999);
c1 :: Classifier(12/9999);

todevice0 :: ToDevice(eth5);
todevice1 :: ToDevice(eth2);

FromDevice(eth5, PROMISC true)  -> c0;

c0[0] -> Strip(14) -> MarkXIAHeader() 
-> Print()
->  XIAPrint("h0->r0")
-> [0]router0; // XIA packet 


FromDevice(eth2, PROMISC true) -> c1;

c1[0] -> Strip(14) -> MarkXIAHeader() 
-> Print()
->  XIAPrint("r1->r0")
-> [1]router0; // XIA packet 

//Idle -> [2]router0[2] -> Discard; 
//Idle -> [3]router0[3] -> Discard; 


router0[0]
-> Print()
->  XIAPrint("r0->h0")
-> EtherEncap(0x9999,  00:04:23:b7:1a:bd, 00:04:23:b7:17:da) 
-> c::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -) 
-> todevice0;


router0[1]
-> Print()
->  XIAPrint("r0->r1")
-> EtherEncap(0x9999, 00:04:23:b7:1a:be,  00:04:23:b7:3e:a3) -> todevice1; 



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





