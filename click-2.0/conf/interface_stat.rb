#!/usr/bin/env ruby
require File.dirname(__FILE__) + '/interface_stat_all.rb'

def netstat(device)
  stats = [Time.new]
  result = `netstat -i `
  result.each_line do |line|
    if (line=~/#{device}/)
       cols = line.split()
       stats[3]= cols[3].to_i()
       stats[4]= cols[7].to_i()
    end 
  end
  return stats
end

def ifconfig(device)
  stats = [Time.new]
  result = `ifconfig #{device}`
  result.each_line do |line|
    if (line=~/RX bytes:/)
       stats[1] = line.split(':')[1].to_i()   # RX bytes
       stats[2] = line.split(':')[2].to_i()   # TX bytes
    end 
    if (line=~/RX packets:/)
       stats[3] = line.split(':')[1].to_i()   # RX pkts
    end 
    if (line=~/TX packets:/)
       stats[4] = line.split(':')[1].to_i()   # TX pkts
    end 
  end
  return stats
end


report_interval = 1

if __FILE__== $0
  if ARGV.size != 2 
     time_interval = 10
     if (ARGV.size==1)
        time_interval= ARGV[0].to_i
     end
     report_all_interface_stats(time_interval, report_interval);
     exit
  end
  device = ARGV[0] 
  time_interval= ARGV[1].to_i
   
  first = []
  first[0] = ifconfig(device)
  first[1] = netstat(device)
  timediff = 0 
  prev = first

  rx = 0
  tx = 0
  rxp = 0 
  txp = 0
  rxp_netstat = 0 
  txp_netstat = 0

  while (timediff+ report_interval/2 < time_interval)
     sleep(report_interval)
     last = [ifconfig(device), netstat(device)]
     timediff= last[0][0]-first[0][0] 

     rx += diff(last[0][1], prev[0][1])
     tx += diff(last[0][2], prev[0][2])
     rxp+= diff(last[0][3], prev[0][3])
     txp+= diff(last[0][4], prev[0][4])
     puts  "#{device} RX " + diff(last[0][3], prev[0][3]).to_s() +" TX "  + diff(last[0][4], prev[0][4]).to_s()

     rxp_netstat+= diff(last[1][3], prev[1][3])
     txp_netstat+= diff(last[1][4], prev[1][4])
     prev = last
  end

  puts "Time interval #{timediff} sec"
  puts "#{device} RX #{"%.2f" % (rx/timediff*8/1000000)} Mbps TX #{"%.2f" % (tx/timediff*8/1000000)} Mbps"
  puts "#{device} RX  #{"%.2f" % (rxp/timediff)} pps TX #{"%.2f" %(txp/timediff)} pps"
  puts "#{device} RX  #{rxp} packets TX #{txp} packets"

  puts "RX_NETSTAT  #{"%.2f" % (rxp_netstat/timediff)} pps TX #{ "%.2f" %(txp_netstat/timediff)} pps"
  puts "RX_NETSTAT  #{rxp_netstat} packets TX #{txp_netstat} packets"
  if (rxp>0)
     avg_pkt_size = rx/rxp
     puts "AVG_SIZE RX #{"%.2f" % (avg_pkt_size)}"
  end
  if (txp>0)
     avg_pkt_size = tx/txp
     puts "AVG_SIZE TX #{"%.2f" % (avg_pkt_size)}"
  end
end
