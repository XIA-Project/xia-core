// udpgen.click

// This file is a simple, fast UDP/IP load generator, meant to be used in the
// Linux kernel module. It sends UDP/IP packets from this machine to another
// machine at a given rate. See 'udpcount.click' for a packet counter
// compatible with udpgen.click.

// The relevant address and rate arguments are specified as parameters to a
// compound element UDPGen.

// UDPGen($device, $rate, $limit, $seth, $sip, $sport, $deth, $dip, $dport);
//
//	$device		name of device to generate traffic on
//	$rate		rate to generate traffic (packets/s)
//	$limit		total number of packets to send
//      $size		bytes per packet
//	$seth		source eth addr
//	$sip		source ip addr
//	$sport		source port
//      $deth		destination eth addr
//	$dip		destination ip addr
//	$dport		destination port

elementclass UDPGen {
  $device, $rate, $limit, $size,
  $seth, $sip, $sport, $deth, $dip, $dport |

  source :: FastUDPSource($rate, $limit, $size, $seth, $sip, $sport,
                                                $deth, $dip, $dport);
  //pd :: FromDevice($device) -> ToHost;
  source -> td :: ToDevice($device);
}

// create a UDPGen

u :: UDPGen(eth0, 89500, 895000, 1400,
	    00:1b:21:01:39:a0, 10.0.1.2, 1234,
	    00:15:17:51:D3:D4, 10.0.2.2, 1234);

