#!/usr/bin/env ruby
#

hdr = "
require(library xia_address.click);
define($PAYLOAD_SIZE 1300);
define($HEADROOM_SIZE 148);  
"


puts hdr
puts ""
puts ""

traffic_matrix = [ [:eth2, :eth4], [:eth3, :eth5], [:eth4, :eth2], [:eth5, :eth3] ]

cnt=1

devices = traffic_matrix.flatten
devices |= devices
devices.each do |dev|
  str = "Q_#{dev}:: Queue() -> ToDevice_#{dev}::ToDevice(#{dev})"
  puts str
  str = "FromDevice(#{dev}, PROMISC true) 
          -> c_#{dev} :: Classifier(12/C0DE, -)
          -> Strip(14) -> MarkXIAHeader()
          -> XIAPrint(\"#{dev} in\") -> Discard; "
  puts ""
  puts str
  puts ""
end
puts ""


traffic_matrix.each do |srcdev, dstdev|
  src = srcdev.to_s().scan(/\d+/)[0]
  dst = dstdev.to_s().scan(/\d+/)[0]
 
  if (src==nil || dst==nil) 
    puts "Parsing error : "+ src
    puts "Parsing error : "+ dst
    exit(-1)
  end

  str = " gen_#{srcdev} :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
	 -> XIAEncap(
    		DST RE  HID#{dst},
    		SRC RE  HID#{src})
	 -> XIAPrint(\"#{srcdev} out\")
	 -> EtherEncap(0xC0DE, 00:1a:92:9b:4a:77 , 00:1a:92:9b:4a:78)  // Just use any ether address
	 //-> Clone($COUNT)
	 -> RatedUnqueue(1)
	 -> Q_#{srcdev}; "

  puts ""
  puts str
  puts ""
  
end

devices.each do |dev|
  puts "c_#{dev}[1]->ToHost;"
  puts "Script(write gen_#{dev}.active true);"
end
