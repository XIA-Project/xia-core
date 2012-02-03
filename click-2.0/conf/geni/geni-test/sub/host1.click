require(library ../../../xia_router_template_xtransport.click);
require(library ../../../xia_address.click);


// host instantiation
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1);
//router1 :: Router4PortDummyCache(RE AD1 RHID1, AD1, RHID1); // if router does not understand CID pricipal


// Interface0 (eth2)
c0 :: Classifier(12/9990 20/0001, 12/9990 20/0002, 12/9999);  // XARP (query) or XARP (response) or XIP
xarpq0 :: XARPQuerier(HID:0000000000000000000000000000000000000001, 00:04:23:b7:1d:f4);
xarpr0 :: XARPResponder(HID:0000000000000000000000000000000000000001 00:04:23:b7:1d:f4);
todevice0 :: ToDevice(eth2);
fromdevice0 :: FromDevice(eth2, PROMISC true);
queue0a :: Queue;
queue0b :: Queue;
unqueue0 :: Unqueue;
mux0 :: DRRSched(2);

// On receiving a packet from Interface0
fromdevice0 -> c0;

// On receiving an XIP packet
c0[2] -> Strip(14) -> MarkXIAHeader() 
-> Print()
-> XIAPrint("r1->h1")
-> [0]host1; // XIA packet

// On receiving XARP response
c0[1] -> [1]xarpq0;
xarpq0 -> queue0a -> [0]mux0;
mux0 -> todevice0;

// On receiving XARP query
c0[0] -> xarpr0;
xarpr0 -> queue0b -> [1]mux0;

// Sending an XIP packet (via XARP if necessary) to Interface0
host1[0]
-> Print()
-> XIAPrint("h1->r1")
-> c::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -) 
-> unqueue0
-> [0]xarpq0




ControlSocket(tcp, 7777);

//Script(write gen.active true);  // the packet source should be activated after all other scripts are executed


//Script(write host1/n/proc/rt_AD/rt.add AD1 4);    
//Script(write host1/n/proc/rt_AD/rt.add - 0); 

//Script(write host1/n/proc/rt_HID/rt.add HID1 4);  
//Script(write host1/n/proc/rt_HID/rt.add - 0);      

//Script(write host1/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
//Script(write host1/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path





