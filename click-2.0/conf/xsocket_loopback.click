// XIA with loopback before packet forwarding

define($AD0 AD:0000000000000000000000000000000000000009);
define($AD1 AD:1000000000000000000000000000000000000009);
define($HID0 HID:0500000000000000000000000000000000000055);
define($HID1 HID:1500000000000000000000000000000000000055);

//Create kernel TAP interface which responds to ARP
fake0::FromHost(fake0,192.0.0.1/24) 
-> fromhost_cl0 :: Classifier(12/0806, 12/0800);
fromhost_cl0[0] -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> ToHost(fake0);

//Classifier to sort between control/normal
fromhost_cl0[1]
->StripToNetworkHeader()
->sorter0::IPClassifier(dst udp port 5001 or 5002 or 5003 or 5004 or 5005 or 5006,
               dst udp port 10000);

//Control in
sorter0[0]
->xudp0::XUDP(RE $AD1 $HID1, 192.0.0.2,192.0.0.1);

//socket side in
sorter0[1]
->[1]xudp0;

//socket side out
xudp0[1]->
cIP0::CheckIPHeader();
cIP0
->EtherEncap(0x0800, 1:1:1:1:1:1, 11:11:11:11:11:11)
-> ToHost(fake0)
cIP0[1]->Print(bad,MAXLENGTH 100, CONTENTS ASCII)->Discard();

//To connect to forwarding instead of loopback
//xudp0[0]->Packet forwarding module
//Packet forwarding module->[2]xudp0;




//loopback side

fake1::FromHost(fake1,172.0.0.1/24) 
-> fromhost_cl1 :: Classifier(12/0806, 12/0800);
fromhost_cl1[0] -> ARPResponder(0.0.0.0/0 2:1:1:1:1:1) -> ToHost(fake1);

//Classifier to sort between control/normal
fromhost_cl1[1]
->StripToNetworkHeader()
->sorter1::IPClassifier(dst udp port 5001 or 5002 or 5003 or 5004 or 5005 or 5006,
               dst udp port 10000);

//Control in
sorter1[0]
//->Print(Gotsomething,MAXLENGTH 100, CONTENTS ASCII)
->xudp1::XUDP(RE $AD0 $HID0, 172.0.0.2,172.0.0.1);;

//socket side in
sorter1[1]
->[1]xudp1;

//socket side out
xudp1[1]
->cIP1::CheckIPHeader();
cIP1
//->Print(Sentsomething,MAXLENGTH 1000, CONTENTS HEX)
//->Print(Sentsomething,MAXLENGTH 1000, CONTENTS ASCII)
->EtherEncap(0x0800, 2:1:1:1:1:1, 11:11:11:11:11:12)
-> ToHost(fake1)
cIP1[1]->Print(bad,MAXLENGTH 100, CONTENTS ASCII)->Discard();

//Connect the loopback
xudp0[2]->[2]xudp1;
xudp1[2]->[2]xudp0;

