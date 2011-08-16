
require(library xia_address.click);
define($PAYLOAD_SIZE 1300);
define($HEADROOM_SIZE 148);  


Q_eth2:: Queue() -> ToDevice_eth2::ToDevice(eth2)

FromDevice(eth2, PROMISC true) 
          -> c_eth2 :: Classifier(12/9999, -)
          -> Strip(14) -> MarkXIAHeader()
          -> XIAPrint("eth2 in") -> Discard; 

Q_eth4:: Queue() -> ToDevice_eth4::ToDevice(eth4)

FromDevice(eth4, PROMISC true) 
          -> c_eth4 :: Classifier(12/9999, -)
          -> Strip(14) -> MarkXIAHeader()
          -> XIAPrint("eth4 in") -> Discard; 

Q_eth3:: Queue() -> ToDevice_eth3::ToDevice(eth3)

FromDevice(eth3, PROMISC true) 
          -> c_eth3 :: Classifier(12/9999, -)
          -> Strip(14) -> MarkXIAHeader()
          -> XIAPrint("eth3 in") -> Discard; 

Q_eth5:: Queue() -> ToDevice_eth5::ToDevice(eth5)

FromDevice(eth5, PROMISC true) 
          -> c_eth5 :: Classifier(12/9999, -)
          -> Strip(14) -> MarkXIAHeader()
          -> XIAPrint("eth5 in") -> Discard; 



 gen_eth2 :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
	 -> XIAEncap(
    		DST RE  HID4,
    		SRC RE  HID2)
	 -> XIAPrint("eth2 out")
	 -> EtherEncap(0x9999, 00:1a:92:9b:4a:77 , 00:1a:92:9b:4a:78)  // Just use any ether address
	 //-> Clone($COUNT)
	 -> RatedUnqueue(1)
	 -> Q_eth2; 


 gen_eth3 :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
	 -> XIAEncap(
    		DST RE  HID5,
    		SRC RE  HID3)
	 -> XIAPrint("eth3 out")
	 -> EtherEncap(0x9999, 00:1a:92:9b:4a:77 , 00:1a:92:9b:4a:78)  // Just use any ether address
	 //-> Clone($COUNT)
	 -> RatedUnqueue(1)
	 -> Q_eth3; 


 gen_eth4 :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
	 -> XIAEncap(
    		DST RE  HID2,
    		SRC RE  HID4)
	 -> XIAPrint("eth4 out")
	 -> EtherEncap(0x9999, 00:1a:92:9b:4a:77 , 00:1a:92:9b:4a:78)  // Just use any ether address
	 //-> Clone($COUNT)
	 -> RatedUnqueue(1)
	 -> Q_eth4; 


 gen_eth5 :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
	 -> XIAEncap(
    		DST RE  HID3,
    		SRC RE  HID5)
	 -> XIAPrint("eth5 out")
	 -> EtherEncap(0x9999, 00:1a:92:9b:4a:77 , 00:1a:92:9b:4a:78)  // Just use any ether address
	 //-> Clone($COUNT)
	 -> RatedUnqueue(1)
	 -> Q_eth5; 

c_eth2[1]->ToHost;
Script(write gen_eth2.active true);
c_eth4[1]->ToHost;
Script(write gen_eth4.active true);
c_eth3[1]->ToHost;
Script(write gen_eth3.active true);
c_eth5[1]->ToHost;
Script(write gen_eth5.active true);
