#!/usr/bin/env ruby
#

def netstat_all()
  stats = []
  result = `netstat -i `
  t = Time.new
  result.each_line do |line|
     #next if !(line=~/eth/)
     #next if (line=~/eth0/)
     #next if (line=~/eth1/)
     #next if (line=~/eth6/)
     #next if (line=~/eth7/)
     next if !(line=~/xge/)
     cols = line.split()
     dev_stats = [t]
     dev_stats[1]= cols[0].to_s() # dev
     dev_stats[2]= cols[3].to_i() # rx
     dev_stats[3]= cols[7].to_i() # tx
     stats.push(dev_stats)
  end
  return stats
end

def diff(a,b)
  d = a-b
  if (a-b <0)
    d+= 2**32 
  end
  return d
end


def report_all_interface_stats(time_interval, report_interval)
  first = netstat_all()
  last = []
  time_diff = 0 
  total_interval = 0  
  prev = first

  rxp_netstat= 0
  txp_netstat= 0

  while (total_interval+ report_interval/2 < time_interval)
     sleep(report_interval)
     last = netstat_all()	

     rxp_netstat_incr= 0
     txp_netstat_incr= 0
    
     last.each_index do |i|
       ptime, pdev, prx, ptx = prev[i]
       time, dev, rx, tx = last[i]
      
       rxp_netstat_incr += diff(rx,prx)
       txp_netstat_incr += diff(tx,ptx)
  
       time_diff = (time - ptime).to_f()
       #puts  "RX " + (diff(rx, prx).to_f()/time_diff).to_s()  +" TX "  + (diff(tx, ptx).to_f()/time_diff).to_s() + " pps"
       
     end
     total_interval = (last[0][0] - first[0][0]).to_f()
     prev = last
     puts "RX pps " + (rxp_netstat_incr.to_f()/time_diff).to_s()  
     puts "TX pps " + (txp_netstat_incr.to_f()/time_diff).to_s() 
     rxp_netstat+=  rxp_netstat_incr
     txp_netstat+=  txp_netstat_incr
  end

  time_diff = (last[0][0] - first[0][0]).to_f()
  puts "Time interval #{time_diff} sec"

  puts "RX_NETSTAT  #{"%.2f" % (rxp_netstat/time_diff)} pps TX #{ "%.2f" %(txp_netstat/time_diff)} pps"
  puts "RX_NETSTAT  #{rxp_netstat} packets TX #{txp_netstat} packets"

end

if __FILE__== $0
  STDOUT.sync = true
  if ARGV.size == 0
     puts "Usage: #{$0} <time_interval> "
     exit
  end
  time_interval= ARGV[0].to_i
  report_interval = 1
  report_all_interface_stats(time_interval, report_interval)  
end
