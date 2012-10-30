#!/usr/bin/env ruby
#

PRINT_PACKET = 1
BURST = 32
NTHREADS = 12
PORTS = [:eth2, :eth3, :eth4, :eth5].map{|s| s.to_s()}.sort()

puts "#!/usr/local/sbin/click-install -uct#{NTHREADS}"
puts ""

# Template and address
hdr = "require(library xia_router_template.click); \n"
hdr+= "require(library xia_address.click); "
puts hdr 

# 4-port router
str = "
// router instantiation
//router0 :: Router4PortDummyCache(RE AD0 RHID0, AD0, RHID0);
router0 :: Router4PortDummyCache(RE AD0 RHID0);
toh :: ToHost; "
puts str

# classifier 
PORTS.each do |dev|
  puts "c_#{dev} :: Classifier(12/C0DE, -);"
end

# Poll device
PORTS.each do |dev|
  puts ""
  0.upto(NTHREADS-1) do |t|
    puts "pd_#{dev}_#{t}:: MQPollDevice(#{dev}, QUEUE #{t}, BURST #{BURST}, PROMISC true) -> c_#{dev}; "
  end
  puts ""
end

# Input to router
PORTS.each do |dev|
  index = PORTS.index(dev)
  puts "c_#{dev}[0] -> Strip(14) -> MarkXIAHeader() -> [#{index}]router0; // XIA packet  "
end

PORTS.each do |dev|
  puts "c_#{dev}[1] -> toh;"
end

PORTS.each do |dev|
  index = PORTS.index(dev)
  str = "router0[#{index}] \n"
  if (PRINT_PACKET!=0)
     str += "-> XIAPrint()"
  end
  str+= " -> encap#{index}::EtherEncap(0xC0DE, 00:15:17:51:d3:d4, 00:1A:92:9B:4A:77); "
  puts str
end


PORTS.each do |dev|
  puts ""
  index = PORTS.index(dev)
  0.upto(NTHREADS-1) do |t|
    puts " encap#{index} -> tod_#{dev}_#{t} :: MQToDevice(#{dev}, QUEUE #{t}, BURST #{BURST}) ";
  end
end


puts ""
puts ""

0.upto(NTHREADS-1) do |t|
   str = "StaticThreadSched("
   first = true
   PORTS.each do |dev|
     if (!first)
 	str+=", "
     end
     str+= "pd_#{dev}_#{t} #{t}, tod_#{dev}_#{t} #{t}"
     first = false
   end
   str+=" );"
   puts str
end

puts ""

PORTS.each do |dev|
  hid_index = dev.to_s().scan(/\d+/)[0]
  index = PORTS.index(dev)
  puts " Script(write router0/n/proc/rt_HID/rt.add HID#{hid_index} #{index});  \/\/ #{dev}'s address "
end


