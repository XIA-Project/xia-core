#!/usr/bin/env ruby

CONF = { :PKT_COUNT => 500000000, :IP_PKT_SIZE=>64, :HEADROOM_SIZE=> 256, :OUTDEVICE=> 'eth2', :BURST=>64  , 
	 :SRC_IP => '10.0.1.2', :SRC_PORT=>5002, :DST_IP=>'10.0.2.2', :DST_PORT=>5002, :SRC_MAC => '00:1a:92:9b:4a:77', :DST_MAC => '00:15:17:51:d3:d4'}

if ( ARGV.size != 1)
   puts "Usage: #{$0} <number_of_threads>"
   exit -1
end

num_threads = ARGV[0].to_i

CONF.each do |k,v|
  puts "define ($" +k.to_s() + " " + v.to_s() + ");"
end

puts "define ($COUNT " + (CONF[:PKT_COUNT]/num_threads).to_i().to_s()  + ");"
puts "define ($PAYLOAD_SIZE " + (CONF[:IP_PKT_SIZE]-28).to_i().to_s()  + ");"

puts ""
puts ""
puts "MQPollDevice($OUTDEVICE) -> Discard;"

1.upto(num_threads) do |n|
  puts ""
  puts ""
  puts "gen#{n}:: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)"
  puts "-> Script(TYPE PACKET, write gen#{n}.active false)       // stop source after exactly 1 packet"
  puts "-> Unqueue()"
  puts "-> UDPIPEncap($SRC_IP, $SRC_PORT, $DST_IP, $DST_PORT)"
  puts "-> EtherEncap(0x0800, $SRC_MAC , $DST_MAC)"

  puts "-> clone#{n} ::Clone($COUNT)"
  puts "-> td#{n} :: MQToDevice($OUTDEVICE, QUEUE #{n-1}, BURST $BURST);"
  puts "StaticThreadSched(td#{n} #{n-1}, clone#{n} #{n-1});"

  puts ""
end


1.upto(num_threads) do |n|
  puts "Script(write gen#{n}.active true);"
end
