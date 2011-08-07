// test-ping.click

// This kernel configuration tests the FromDevice and ToDevice elements
// by sending pings to host 131.179.80.139 (read.cs.ucla.edu) via 'eth0'.
// Change the 'define' statement to use another device or address, or run
// e.g. "click-install test-ping.click DEV=eth1" to change a parameter at the
// command line.
//
// You should see, in 'dmesg' or /var/log/messages, a sequence of "icmp echo"
// printouts intermixed with "ping :: ICMPPingSource" receive reports.  Also
// check out the contents of /click/ping/summary.

define($DEV eth1, $DADDR 131.179.80.139,  $REPDEV eth2)

FromDevice($REPDEV, PROMISC true)
	-> c2 :: Classifier(12/0800, 12/0806 20/0001,  -)
	-> CheckIPHeader(14)
	-> ip2 :: IPClassifier(icmp echo, -)
	//-> ICMPPingResponder()
	//-> arpq2 :: ARPQuerier($DEV)
	-> IPPrint($REPDEV)
	//-> Strip(14)
	-> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2)
	-> q2 :: Queue
	-> ToDevice($REPDEV);

//arpq2[1]-> q2;
//c2[1]	-> t2 :: Tee
//	-> [1] arpq2;
//t2[1]	-> host :: ToHost;
c2[1]   ->  ARPResponder($DADDR 00:1b:21:a3:d6:a8) -> q2;
c2[2]	-> host :: ToHost;
ip2[1]	-> host;

FromDevice($DEV)
	-> c :: Classifier(12/0800, -)
	-> CheckIPHeader(14)
	-> ip :: IPClassifier(icmp echo-reply, -)
	-> ping :: ICMPPingSource($DEV, $DADDR) // send ICMP Request
	//-> SetIPAddress($GW)
	//-> arpq :: ARPQuerier($DEV)
	-> IPPrint($DEV)
	-> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2)
	-> q :: Queue
	-> { input -> t :: PullTee -> output; t [1] -> ToHostSniffers($DEV) }
	-> ToDevice($DEV);
//arpq[1]	-> q;
//c[1]	-> t :: Tee
//	-> [1] arpq;
//t[1]	-> host;  //:: ToHost;
c[1]	-> host;
ip[1]	-> host;
