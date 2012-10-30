#!/usr/bin/env ruby

ncpu=ARGV[0].to_i 
puts "#!/usr/local/sbin/click-install -uct#{ncpu}"
puts "
define ($RX_BURST 32);
define ($TX_BURST 32); "

0.upto(ncpu-1) do |i|
  [:eth2, :eth4].each do |dev|
    dev= dev.to_s
    puts "
pd_#{dev}_#{i}:: MQPollDevice(#{dev}, QUEUE #{i}, BURST $RX_BURST, PROMISC true)  ->
   td_#{dev}_#{i}::MQPushToDevice(#{dev}, QUEUE #{i}, BURST $TX_BURST);
StaticThreadSched(pd_#{dev}_#{i} #{i});

    "
  end
end

