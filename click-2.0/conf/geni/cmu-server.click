require(library ../xia_router_template_xtransport.click);
require(library ../xia/xia_address.click);


// host & router instantiation
host2 :: EndHost (RE AD_CMU HID2, HID2, fake2, 212.0.0.2,212.0.0.1,31:11:11:11:11:11,1);
router2 :: Router(RE AD_CMU RHID2, AD_CMU, RHID2);

c0 :: Classifier(12/0800);

//todevice0 :: ToDevice(eth0);


// From Internet (GENI)
FromDevice(eth0, PROMISC true) -> c0;
c0[0] 
-> Strip(14) -> MarkIPHeader()
-> c_ip::IPClassifier(udp port 9999)
-> IPPrint("incoming")
-> Strip(28)
-> MarkXIAHeader()
->  XIAPrint("router1->AD_CMU")
->[1]router2;


// To GENI
router2[1]
->  XIAPrint("AD_CMU->router1")
-> c::XIAXIDTypeCounter(src AD, src HID, src SID, src CID, src IP, -)
-> Socket(UDP, 155.98.39.123, 9999, 0.0.0.0, 9999, SNAPLEN 9000) // DestIP, DestPort, SrcIP, SrcPort  pg42.emulab.net

ControlSocket(tcp, 7777);


// interconnection -- host - ad
host2[0] 
->  XIAPrint("CMU_HOST->AD_CMU")
//->  XIAPrint("h0->r0") -> c::XIAXIDTypeCounter(dst AD, dst HID, dst SID, dst CID, dst IP, -)
-> LinkUnqueue(0.005, 1 GB/s) -> [0]router2;

router2[0] 
->  XIAPrint("AD_CMU->CMU_HOST")
//-> XIAPrint("r0->h0")
-> LinkUnqueue(0.005, 1 GB/s) -> [0]host2;


Script(write router2/n/proc/rt_CID/rt.add CID20 0);     // hack: due to the current hardcoded fallback from proxy
Script(write router2/n/proc/rt_CID/rt.add CID21 0);     // hack: due to the current hardcoded fallback from proxy
Script(write router2/n/proc/rt_CID/rt.add CID22 0);     // hack: due to the current hardcoded fallback from proxy
Script(write router2/n/proc/rt_CID/rt.add CID23 0);     // hack: due to the current hardcoded fallback from proxy
