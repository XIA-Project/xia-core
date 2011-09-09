#!/usr/bin/env ruby

require File.dirname(__FILE__) + '/interface_stat.rb'


if __FILE__== $0
  if ARGV.size != 1
     puts "Usage: #{$0}  <time_interval> "
     exit
  end
  time_interval= ARGV[0].to_i

  cnt =`cat /proc/click/tod*/count 2>/dev/null| wc`
  cmd = "cat /proc/click/tod*/count 2>/dev/null"
  cmd_drop = "cat /proc/click/tod*/drop 2>/dev/null"
  
  if (cnt.to_i()==0)
    cnt =`cat /proc/click/td*/count 2>/dev/null| wc`
    cmd = "cat /proc/click/td*/count 2>/dev/null"
    cmd_drop = "cat /proc/click/td*/drop 2>/dev/null"
  end

  if (cnt.to_i()==0)
    cnt =`cat /click/ge*/gen_sub*/td*/count 2>/dev/null| wc`
    cmd = "cat /click/ge*/gen_sub*/td*/count 2>/dev/null"
    cmd_drops = "cat /click/ge*/gen_sub*/td*/drops 2>/dev/null"
  end

  if (cnt.to_i()==0)
    cmd = "cat /click/router@*/td*/count  2>/dev/null"
    cmd_drops = "cat /click/router@*/td*/drops 2>/dev/null"
  end
 
  rx_q_cnt = 0 
  rx_q_cnt =`cat /proc/click/pd*/count 2>/dev/null| wc`
  cmd_rx = "cat /proc/click/pd*/count 2>/dev/null"
  cmd_rx_drop = "cat /proc/click/pd*/drop 2>/dev/null"

  if (rx_q_cnt==0) 
     cmd_rx = "cat /proc/click/router*/pd*/count 2>/dev/null"
     cmd_rx_drop = "cat /proc/click/router*/pd*/drop 2>/dev/null"
  end

  report_interval = 1
  total_prev = `#{cmd} | awk '{SUM+=$1} END {print SUM}'`
  total_prev = total_prev.to_f
  total_prev_drop = `#{cmd_drop} | awk '{SUM+=$1} END {print SUM}'`
  total_prev_drop = total_prev_drop.to_f

  total_prev_rx = `#{cmd_rx} | awk '{SUM+=$1} END {print SUM}'`
  total_prev_rx = total_prev_rx.to_f
  total_prev_rx_drop = `#{cmd_rx_drop} | awk '{SUM+=$1} END {print SUM}'`
  total_prev_rx_drop = total_prev_rx_drop.to_f
  start = Time.new
  prev = start
  now = start
  tx = 0
  tx_drop =0
  rx = 0
  rx_drop =0
  total_duration = 0 

  while (total_duration+ report_interval/2 < time_interval)
    sleep(report_interval)
    total = `#{cmd} | awk '{SUM+=$1} END {print SUM}'`
    total = total.to_f()
    total_drop = `#{cmd_drop}  | awk '{SUM+=$1} END {print SUM}'`
    total_drop = total_drop.to_f

    total_rx = `#{cmd_rx} | awk '{SUM+=$1} END {print SUM}'`
    total_rx = total_rx.to_f()
    total_rx_drop = `#{cmd_rx_drop}  | awk '{SUM+=$1} END {print SUM}'`
    total_rx_drop = total_rx_drop.to_f

    # time 
    now = Time.new
    total_duration= now -start
    interval = now -prev

    # TX
    sent = diff(total, total_prev)
    sent_drop = diff(total_drop, total_prev_drop)
    tx+= sent
    tx_drop+= sent_drop
    puts "TX pkts/sec " + (sent.to_f()/interval).to_s()  + " drop " + (sent_drop.to_f()/interval).to_s + " Duration " + interval.to_s() + " sec "
    total_prev = total
    total_prev_drop = total_drop

    # RX
    recv= diff(total_rx, total_prev_rx)
    recv_drop = diff(total_rx_drop, total_prev_rx_drop)
    rx+= recv 
    rx_drop+= recv_drop
    puts "RX pkts/sec " + (recv.to_f()/interval).to_s()  + " drop " + (recv_drop.to_f()/interval).to_s + " Duration " + interval.to_s() + " sec "

    total_prev_rx = total_rx
    total_prev_rx_drop = total_rx_drop

    prev = now

  end
  puts "TX pkts/sec  " + (tx.to_f()/(now-start)).to_s() + " Drop " +(tx_drop.to_f()/(now-start)).to_s() + " Duration " + (now-start).to_s() + " sec"
  puts "RX pkts/sec  " + (rx.to_f()/(now-start)).to_s() + " Drop " +(rx_drop.to_f()/(now-start)).to_s() + " Duration " + (now-start).to_s() + " sec"
end
