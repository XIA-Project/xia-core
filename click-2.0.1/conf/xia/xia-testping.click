// test-ping.click

// This kernel configuration tests the FromDevice and ToDevice elements
// by sending pings to host 131.179.80.139 (read.cs.ucla.edu) via 'eth0'.
// Change the 'define' statement to use another device or address, or run
// e.g. "click-install test-ping.click DEV=eth1" to change a parameter at the
// command line.
//
// You should see, in /var/log/sysmlog, a sequence of "icmp echo"
// printouts. If the router is working well, you should see icmp echo
// going out from $DEV and coming back to $REPDEV.
// $DADDR should be in the routing table in the router.

define($DEV eth3, $REPDEV eth5, $DADDR 10.0.3.2 )

FromDevice($REPDEV, PROMISC true)
	-> c2 :: Classifier(12/0800, 12/0806 20/0001,  -)
	-> CheckIPHeader(14)
	-> ip2 :: IPClassifier(icmp echo, -)
	-> IPPrint($REPDEV)
	-> Discard;

//c2[1]   -> ARPResponder($REPDEV 00:1b:21:a3:d6:a8) -> Queue() ->ToDevice($REPDEV);
c2[1]   -> ARPResponder($REPDEV $REPDEV) -> Queue() ->ToDevice($REPDEV);
c2[2]	-> host :: ToHost;
ip2[1]	-> host;

ping :: ICMPPingSource($DEV, $REPDEV) // send ICMP Request
-> IPPrint($DEV)
-> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2)
-> q :: Queue
-> { input -> t :: PullTee -> output; t [1] -> ToHostSniffers($DEV) }
-> ToDevice($DEV);

