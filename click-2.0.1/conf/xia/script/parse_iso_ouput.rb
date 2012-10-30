#!/usr/bin/env ruby
require 'parse_output.rb'

def read_txq_stat(file)
  stat = []
  File.new(file).each_line do |line|
    next if (!(line=~/tx_queue/))
    next if (!(line=~/packets/))
    qname, packets = line.split
    stat.push(packets.to_i)
  end 
  return stat
end

def calc_tx_stat(ethtool_start_output, ethtool_end_output)
  start_stat = read_txq_stat(ethtool_start_output)
  end_stat = read_txq_stat(ethtool_end_output)
  start_stat.each_index do |i|
    end_stat[i] -=start_stat[i]
  end
  return end_stat
end

if __FILE__==$0
  Dir.chdir("output")

  file_glob = "XIA-256-isolation-*"
  Pathname.glob(file_glob).sort {|x,y| x.to_s.split('-')[3].to_i() <=>  y.to_s.split('-')[3].to_i()}.each do |f|
    proto, _, _, num_resources = f.basename().to_s.split('-')
    sum_total = 0
    sum_cid = 0
 
    (0..3).to_a.each do |i|
      dev = "xge#{i}"
      ethtool_start_output = f.most_recent_file(/ethtool-begin-#{dev}/)
      ethtool_end_output = f.most_recent_file(/ethtool-end-#{dev}/)
      tx_stat_per_q = calc_tx_stat(ethtool_start_output, ethtool_end_output)
      cid = 0
      1.upto(num_resources.to_i) do |q|
        cid+=tx_stat_per_q[q]
      end 
      total = tx_stat_per_q.sum
      sum_total+=total
      sum_cid+=cid
    end
    puts "#{num_resources} #{sum_total} #{sum_cid} #{sum_cid.to_f()/sum_total}"

  end
end
