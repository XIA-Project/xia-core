require(library xia_two_port_four_port_router.click); 
require(library ../xia/xia_address.click);


// host & router instantiation
host2 :: EndHost (RE AD_CMU HID2, HID2, fake2, 212.0.0.2,212.0.0.1,31:11:11:11:11:11,1);
router2 :: Router(RE AD_CMU RHID2, AD_CMU, RHID2);

c0 :: Classifier(12/9999, 12/0800);

todevice0 :: ToDevice(eth0);


// From Internet (GENI)
FromDevice(eth0, PROMISC true) -> c0;
c0[1] -> Strip(14) -> MarkIPHeader()
-> c_ip::IPClassifier(udp port 9999)
-> StripIPHeader()
-> MarkXIAHeader()
->[1]router2;


// To GENI
router2[1]
-> Socket(UDP, 64.57.23.165, 8027, 128.2.208.168, 9999, SNAPLEN 9000) // pg40.emulab.net



// interconnection -- host - ad
host2[0] 
//->  XIAPrint("h0->r0") -> c::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
-> LinkUnqueue(0.005, 1 GB/s) -> [0]router2;

router2[0] 
//-> XIAPrint("r0->h0")
-> LinkUnqueue(0.005, 1 GB/s) -> [0]host2;




