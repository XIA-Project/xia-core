require(library xia_two_port_four_port_router.click); 

// router instantiation

router1 :: Router4Port (RE AD1 RHID1); 


c0 :: Classifier(12/9999, 12/0800);
c1 :: Classifier(12/9999, 12/0800);
c2 :: Classifier(12/9999, 12/0800);

todevice0 :: ToDevice(eth2);
todevice1 :: ToDevice(eth1);

// From device (GENI pg40)
FromDevice(eth2, PROMISC true) -> c0;
c0[0] -> Strip(14) -> MarkXIAHeader() 
->  XIAPrint("pg40->pg42")
-> [0]router1; // XIA packet

// From Internet (CMU)
c0[1] 
-> Strip(14) -> MarkIPHeader()
-> c_ip::IPClassifier(udp port 9999)
-> Print()
-> StripIPHeader()
-> Strip(28)
-> MarkXIAHeader()
->  XIAPrint("CMU->pg42")
->[2]router1; 


// From device (GENI pg55)
FromDevice(eth1, PROMISC true) -> c1;
c1[0] -> Strip(14) -> MarkXIAHeader() 
->  XIAPrint("pg55->pg42")
-> [1]router1; // XIA packet

// From Internet (CMU)
c1[1] 
-> Strip(14) -> MarkIPHeader()
-> c_ip1::IPClassifier(udp port 9999)
-> Print()
-> StripIPHeader()
-> Strip(28)
-> MarkXIAHeader()
->  XIAPrint("CMU->pg42")
->[2]router1; 


// From Internet (CMU)
FromDevice(eth0, PROMISC true) -> c2;
c2[1] 
-> Strip(14) -> MarkIPHeader()
-> c_ip2::IPClassifier(udp port 9999)
-> IPPrint("incoming")
-> Strip(28)
-> MarkXIAHeader()
->  XIAPrint("CMU->pg42")
-> bc::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -) 
->[2]router1; 


ControlSocket(tcp, 7000);


Idle -> c2[0] -> Discard;


// To device0 (GENI pg40)
router1[0]
//-> XIAPrint() 
-> EtherEncap(0x9999, 00:1B:21:3A:0E:D0, 00:1B:21:3A:D5:99) -> todevice0;


// To device1 (GENI pg55)
router1[1]
//-> XIAPrint() 
-> EtherEncap(0x9999, 00:24:E8:30:AD:59, 00:1b:21:3a:d7:50) 
-> c::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -)
-> todevice1; 


// To CMU
router1[2]
->  XIAPrint("pg42->CMU")
-> Socket(UDP, 128.2.208.168, 9999, 0.0.0.0, 9999, SNAPLEN 9000) // ng2.nan.cs.cmu.edu



Idle -> [3]router1[3] -> Discard; 



ControlSocket(tcp, 7777);


Script(write router1/n/proc/rt_AD/rt.add - 1);
Script(write router1/n/proc/rt_AD/rt.add AD0 1);
Script(write router1/n/proc/rt_AD/rt.add AD_CMU 2);  
Script(write router1/n/proc/rt_AD/rt.add AD1 4); 

Script(write router1/n/proc/rt_HID/rt.add - 0);  
Script(write router1/n/proc/rt_HID/rt.add RHID1 4); 
 
Script(write router1/n/proc/rt_SID/rt.add - 5);     // no default route for SID; consider other path
Script(write router1/n/proc/rt_CID/rt.add - 5);     // no default route for CID; consider other path



