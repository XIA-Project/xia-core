require(library ../../../xia_router_template_xtransport.click);
require(library ../../../xia_address.click);


// router instantiation
router1 :: Router(RE AD1 RHID1, AD1, RHID1);
//router1 :: Router4Port(RE AD1 RHID1);
//router1 :: Router4PortDummyCache(RE AD1 RHID1, AD1, RHID1); // if router does not understand CID pricipal


// Interface0 (eth4)
c0 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
xarpq0 :: XARPQuerier(HID:0000000000000000000000000000000000000003, 00:04:23:b7:41:50);
xarpr0 :: XARPResponder(HID:0000000000000000000000000000000000000003 00:04:23:b7:41:50);
todevice0 :: ToDevice(eth4);
fromdevice0 :: FromDevice(eth4, PROMISC true);
queue0a :: Queue;
queue0b :: Queue;
unqueue0 :: Unqueue;
mux0 :: DRRSched(2);

// On receiving a packet from Interface0
fromdevice0 -> c0;

// On receiving an XIP packet
c0[2] -> Strip(14) -> MarkXIAHeader() 
-> Print()
-> XIAPrint("h1->r1")
-> [0]router1; // XIA packet

// On receiving XARP response
c0[1] -> [1]xarpq0;
xarpq0 -> queue0a -> [0]mux0;
mux0 -> todevice0;

// On receiving XARP query
c0[0] -> xarpr0;
xarpr0 -> queue0b -> [1]mux0;

// Sending an XIP packet (via XARP if necessary) to Interface0
router1[0]
-> Print()
-> XIAPrint("r1->h1")
-> cnt0::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -) 
-> unqueue0
-> [0]xarpq0





// Interface1 (eth2)
c1 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
xarpq1 :: XARPQuerier(HID:0000000000000000000000000000000000000003, 00:04:23:b7:3f:ce);
xarpr1 :: XARPResponder(HID:0000000000000000000000000000000000000003 00:04:23:b7:3f:ce);
todevice1 :: ToDevice(eth2);
fromdevice1 :: FromDevice(eth2, PROMISC true);
queue1a :: Queue;
queue1b :: Queue;
unqueue1 :: Unqueue;
mux1 :: DRRSched(2);

// On receiving a packet from Interface1
fromdevice1 -> c1;

// On receiving an XIP packet
c1[2] -> Strip(14) -> MarkXIAHeader() 
-> Print()
-> XIAPrint("r0->r1")
-> [1]router1; // XIA packet

// On receiving XARP response
c1[1] -> [1]xarpq1;
xarpq1 -> queue1a -> [0]mux1;
mux1 -> todevice1;

// On receiving XARP query
c1[0] -> xarpr1;
xarpr1 -> queue1b -> [1]mux1;

// Sending an XIP packet (via XARP if necessary) to Interface1
router1[1]
-> Print()
-> XIAPrint("r1->r0")
-> cnt1::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -) 
-> unqueue1
-> [0]xarpq1







ControlSocket(tcp, 7777);


//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed


//Script(write router1/n/proc/rt_HID/rt.add HID1 1);    
//Script(write router1/n/proc/rt_HID/rt.add HID2 2);  
//Script(write router1/n/proc/rt_HID/rt.add - 5);    

//Script(write router1/n/proc/rt_AD/rt.add AD0 0);     
//Script(write router1/n/proc/rt_AD/rt.add AD1 4);    
//Script(write router1/n/proc/rt_AD/rt.add - 5);   

//Script(write router1/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write router1/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path





