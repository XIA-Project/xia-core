Q_eth2:: Queue() -> ToDevice_eth2::ToDevice(eth2)
Q_eth4:: Queue() -> ToDevice_eth4::ToDevice(eth4)
Q_eth3:: Queue() -> ToDevice_eth3::ToDevice(eth3)
Q_eth5:: Queue() -> ToDevice_eth5::ToDevice(eth5)
FromDevice(eth4, PROMISC true)
  	-> c1 :: Classifier(12/0800, 12/0806 20/0001,  -)
  	-> CheckIPHeader(14)
  	-> ip1 :: IPClassifier(icmp echo, -)
  	-> IPPrint("IN eth4")
  	-> Discard; 

c1[1]   -> ARPResponder(eth4 eth4) -> Q_eth4;
  	 c1[2]	-> host1 :: ToHost(eth4);
  	 ip1[1]	-> host1;

ICMPPingSource(eth2, eth4, INTERVAL 0.1) // send ICMP Request
  -> IPPrint("OUT eth2")
  -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2)
  -> Q_eth2
  //-> { input -> t1 :: PullTee -> output; t1[1] -> ToHostSniffers(eth2) }
  //-> ToDevice_eth2; 


FromDevice(eth5, PROMISC true)
  	-> c2 :: Classifier(12/0800, 12/0806 20/0001,  -)
  	-> CheckIPHeader(14)
  	-> ip2 :: IPClassifier(icmp echo, -)
  	-> IPPrint("IN eth5")
  	-> Discard; 

c2[1]   -> ARPResponder(eth5 eth5) -> Q_eth5;
  	 c2[2]	-> host2 :: ToHost(eth5);
  	 ip2[1]	-> host2;

ICMPPingSource(eth3, eth5) // send ICMP Request
  -> IPPrint("OUT eth3")
  -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2)
  -> Q_eth3
  //-> { input -> t2 :: PullTee -> output; t2[1] -> ToHostSniffers(eth3) }
  //-> ToDevice_eth3; 


FromDevice(eth2, PROMISC true)
  	-> c3 :: Classifier(12/0800, 12/0806 20/0001,  -)
  	-> CheckIPHeader(14)
  	-> ip3 :: IPClassifier(icmp echo, -)
  	-> IPPrint("IN eth2")
  	-> Discard; 

c3[1]   -> ARPResponder(eth2 eth2) -> Q_eth2;
  	 c3[2]	-> host3 :: ToHost(eth2);
  	 ip3[1]	-> host3;

ICMPPingSource(eth4, eth2) // send ICMP Request
  -> IPPrint("OUT eth4")
  -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2)
  -> Q_eth4
  //-> { input -> t3 :: PullTee -> output; t3[1] -> ToHostSniffers(eth4) }
  //-> ToDevice_eth4; 


FromDevice(eth3, PROMISC true)
  	-> c4 :: Classifier(12/0800, 12/0806 20/0001,  -)
  	-> CheckIPHeader(14)
  	-> ip4 :: IPClassifier(icmp echo, -)
  	-> IPPrint("IN eth3")
  	-> Discard; 

c4[1]   -> ARPResponder(eth3 eth3) -> Q_eth3;
  	 c4[2]	-> host4 :: ToHost(eth3);
  	 ip4[1]	-> host4;

ICMPPingSource(eth5, eth3) // send ICMP Request
  -> IPPrint("OUT eth5")
  -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2)
  -> Q_eth5
  //-> { input -> t4 :: PullTee -> output; t4[1] -> ToHostSniffers(eth5) }
  //-> ToDevice_eth5; 


