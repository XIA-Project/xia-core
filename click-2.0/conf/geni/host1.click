require(library ../xia_router_template_xtransport.click);
require(library ../xia/xia_address.click);

// host instantiation
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1);
//router1 :: Router4PortDummyCache(RE AD1 RHID1, AD1, RHID1); // if router does not understand CID pricipal

c0 :: Classifier(12/9999);

todevice0 :: ToDevice(eth2);

FromDevice(eth2, PROMISC true) -> c0;
c0[0] -> Strip(14) -> MarkXIAHeader() 
->  XIAPrint("router1->host1")
-> [0]host1; // XIA packet 



host1[0]
//-> XIAPrint() 
-> EtherEncap(0x9999, 00:04:23:b7:19:02, 00:04:23:b7:40:76) 
//-> Print() 
->  XIAPrint("host1->router1")
-> c::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -)
-> todevice0;

ControlSocket(tcp, 7777);

//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed


//Script(write host1/n/proc/rt_AD/rt.add AD1 4);    
//Script(write host1/n/proc/rt_AD/rt.add - 0); 

//Script(write host1/n/proc/rt_HID/rt.add HID1 4);  
//Script(write host1/n/proc/rt_HID/rt.add - 0);      

//Script(write host1/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write host1/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path





