require 'interface_stat.rb'

#cnt = `cat /proc/click/tod*/count| wc`

if __FILE__== $0
  if ARGV.size != 1
     puts "Usage: #{$0}  <time_interval> "
     exit
  end
  time_interval= ARGV[0].to_i

  report_interval = 1
  total_prev = `cat /proc/click/tod*/count | awk '{SUM+=$1} END {print SUM}'`
  total_prev = total_prev.to_i
  total_prev_drop = `cat /proc/click/tod*/drops | awk '{SUM+=$1} END {print SUM}'`
  total_prev_drop = total_prev_drop.to_i
  start = Time.new
  prev = start
  now = start
  tx = 0
  tx_drop =0
  timediff = 0 

  while (timediff+ report_interval/2 < time_interval)
    sleep(report_interval)
    total = `cat /proc/click/tod*/count | awk '{SUM+=$1} END {print SUM}'`
    total = total.to_i()
    total_drop = `cat /proc/click/tod*/drops | awk '{SUM+=$1} END {print SUM}'`
    total_drop = total_drop.to_i
    now = Time.new
    timediff= now -start
    duration = now -prev
    diff = diff(total, total_prev)
    diff_drop = diff(total_drop, total_prev_drop)
    tx+= diff
    tx_drop+= diff_drop
    puts "TX pkts/sec " + (diff.to_f()/duration).to_s()  + " drop " + (diff_drop.to_f()/duration).to_s + " Duration " + duration.to_s() + " sec "
    prev = now
    total_prev = total
    total_prev_drop = total_drop
  end
  puts "TX pkts/sec  " + (tx.to_f()/(now-start)).to_s() + " Drop " +(tx_drop/(now-start)).to_s() + " Duration " + (now-start).to_s() + " sec"
end
