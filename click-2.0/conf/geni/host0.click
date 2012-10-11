require(library ../xia_router_template_xtransport.click);
require(library ../xia/xia_address.click);

// host instantiation
host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,0);
//router1 :: Router4PortDummyCache(RE AD1 RHID1, AD1, RHID1); // if router does not understand CID pricipal

c0 :: Classifier(12/C0DE);

todevice0 :: ToDevice(eth2);

FromDevice(eth2) -> c0;
c0[0] -> Strip(14) -> MarkXIAHeader() 
->  XIAPrint("router0->host0")
-> [0]host0; // XIA packet 


host0[0]
//-> XIAPrint() 
-> EtherEncap(0xC0DE, 00:04:23:b7:1b:e2, 00:04:23:b7:1a:bd) 
->  XIAPrint("host0->router0")
-> c::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
-> todevice0;

ControlSocket(tcp, 7777);

//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed


//Script(write host0/n/proc/rt_HID/rt.add HID0 4);  
//Script(write host0/n/proc/rt_HID/rt.add - 0);    

//Script(write host0/n/proc/rt_AD/rt.add AD0 4);    
//Script(write host0/n/proc/rt_AD/rt.add - 0);   

//Script(write host0/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write host0/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path





