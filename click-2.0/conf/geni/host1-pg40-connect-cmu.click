require(library xia_two_port_four_port_router.click); 


// host instantiation
host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1);
router1 :: Router4Port (RE AD1 RHID2); 

c0 :: Classifier(12/9999, 12/0800);

todevice0 :: ToDevice(eth3);

// From device (GENI)
FromDevice(eth3, PROMISC true) -> c0;
c0[0] -> Strip(14) -> MarkXIAHeader() 
->  XIAPrint("pg42->pg40")
-> [0]router1; // XIA packet 

// From Internet (CMU)
c0[1] -> Strip(14) -> MarkIPHeader()
-> c_ip::IPClassifier(udp port 9999)
-> StripIPHeader()
-> MarkXIAHeader()
->[1]router1;

// From host1
host1[0]-> Unqueue()-> [2]router1;

// Null
Idle->[3]router1;

// To device (GENI0)
router1[0]
//-> XIAPrint() 
-> EtherEncap(0x9999, 00:1B:21:3A:D5:99, 00:1B:21:3A:0E:D0) 
//-> Print() 
-> c::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -)
-> todevice0;

// To CMU
router1[1]
->  XIAPrint("pg40->CMU")
-> Socket(UDP, 128.2.208.168, 9999, 0.0.0.0, 9999, SNAPLEN 9000) // ng2.nan.cs.cmu.edu

// To host1
router1[2] -> Unqueue()-> host1

// Null
router1[3]->Discard;

ControlSocket(tcp, 7777);


Script(write router1/n/proc/rt_AD/rt.add AD0 0);  
Script(write router1/n/proc/rt_AD/rt.add AD_CMU 1);  
Script(write router1/n/proc/rt_HID/rt.add HID1 2);  
Script(write router1/n/proc/rt_AD/rt.add AD1 4);  
Script(write router1/n/proc/rt_HID/rt.add RHID2 4);  
Script(write router1/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
Script(write router1/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path
