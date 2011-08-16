#!/usr/bin/env ruby

KERNEL_MODULE = 0
traffic_matrix = [ [:eth2, :eth4], [:eth3, :eth5], [:eth4, :eth2], [:eth5, :eth3] ]


cnt=1

devices = traffic_matrix.flatten
devices |= devices
devices.each do |dev|
  str = "Q_#{dev}:: Queue() -> ToDevice_#{dev}::ToDevice(#{dev})"
  puts str
end

traffic_matrix.each do | SRCDEV, DSTDEV |
  # Print out incoming packets
  str = "FromDevice(#{DSTDEV}, PROMISC true)
  	-> c#{cnt} :: Classifier(12/0800, 12/0806 20/0001,  -)
  	-> CheckIPHeader(14)
  	-> ip#{cnt} :: IPClassifier(icmp echo, -)
  	-> IPPrint(\"IN #{DSTDEV}\")
  	-> Discard; "
  
  puts str
  #
  # Respond to ARP
  if (KERNEL_MODULE==1)
    str = "c#{cnt}[1]   -> ARPResponder(#{DSTDEV} #{DSTDEV}) -> Q_#{DSTDEV};
    	   c#{cnt}[2]	-> host#{cnt} :: ToHost;
  	   ip#{cnt}[1]	-> host#{cnt};"
  else
    str = "c#{cnt}[1]   -> ARPResponder(#{DSTDEV} #{DSTDEV}) -> Q_#{DSTDEV};
  	 c#{cnt}[2]	-> host#{cnt} :: ToHost(#{DSTDEV});
  	 ip#{cnt}[1]	-> host#{cnt};"
  end
  
  puts ""
  puts str
  puts ""
  
  # Finally, send a packet to the router#
  
  str  = "ICMPPingSource(#{SRCDEV}, #{DSTDEV}) // send ICMP Request
  -> IPPrint(\"OUT #{SRCDEV}\")
  -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2)
  -> Q_#{SRCDEV}
  //-> { input -> t#{cnt} :: PullTee -> output; t#{cnt}[1] -> ToHostSniffers(#{SRCDEV}) }
  //-> ToDevice_#{SRCDEV}; "
  
  puts str
  puts ""
  puts ""
  cnt+=1
end
