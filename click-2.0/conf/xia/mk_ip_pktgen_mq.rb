#!/usr/bin/env ruby

CONF = { :PKT_COUNT => 500000000, :IP_PKT_SIZE=>64, :HEADROOM_SIZE=> 256, 
	 :BURST=>64  , 
	 :SRC_PORT=>5012, :DST_PORT=>5002, 
	 :SRC_MAC => '00:1a:92:9b:4a:77', :DST_MAC => '00:15:17:51:d3:d4'}

if ( ARGV.size != 1)
   puts "Usage: #{$0} <number_of_threads>"
   exit -1
end

traffic_matrix = { :eth2=> :eth4, :eth3=> :eth5, :eth4=>:eth2 ,:eth5 => :eth3 }

devices = traffic_matrix.to_a().flatten
devices |= devices
devices = devices.map {|d| d.to_s}.sort()

num_threads = ARGV[0].to_i

CONF.each do |k,v|
  puts "define ($" +k.to_s() + " " + v.to_s() + ");"
end

puts "define ($COUNT " + (CONF[:PKT_COUNT]/num_threads).to_i().to_s()  + ");"
puts "define ($PAYLOAD_SIZE " + (CONF[:IP_PKT_SIZE]-28).to_i().to_s()  + ");"

puts ""
puts ""
devices.each do |dev|
  puts "MQPollDevice(#{dev}) -> Discard;"
end
puts ""

1.upto(num_threads) do |n|
  dev_index = 0
  dev = devices[dev_index]

  if (num_threads>=devices.size)
    dev_index = (n-1)/(num_threads/devices.size)
    dev = devices[dev_index]
  end
  src_ip = dev
  dst_ip = traffic_matrix[dev.to_sym]

  puts ""
  puts ""
  puts "gen#{n}:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)"
  puts "-> Script(TYPE PACKET, write gen#{n}.active false)       // stop source after exactly 1 packet"
  puts "-> Unqueue()"
  puts "-> UDPIPEncap(#{src_ip}, $SRC_PORT, #{dst_ip}, $DST_PORT)"
  puts "-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)"
  puts "-> CheckIPHeader(14)"
  puts "-> IPPrint(gen#{n}_#{dev})"
  puts "-> clone#{n} ::Clone($COUNT)"
  puts "-> td#{n} :: MQToDevice(#{dev}, QUEUE #{n-1}, BURST $BURST);"
  puts "StaticThreadSched(td#{n} #{n-1}, clone#{n} #{n-1});"

  puts ""
end


1.upto(num_threads) do |n|
  puts "Script(write gen#{n}.active true);"
end
