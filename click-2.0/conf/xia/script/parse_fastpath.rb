#!/usr/bin/env ruby 
require 'parse_output'

def parse_fp_result(proto)
  output = $stdout
  pwd = Pathname.getwd() 
  line = 0
  #["SP", "FP"].each do |p|
    file_glob = "#{proto}-192-*P"
    Pathname.glob(file_glob).sort {|x,y| x.to_s.split('-')[2]+x.to_s.split('-')[3] <=>  y.to_s.split('-')[2].to_s() + y.to_s.split('-')[3].to_s()}.each do |f|
      proto, length, fb, fastpath = f.basename().to_s.split('-')
      statfiles = f.most_recent_file(/stat-/, true)
      count=0
      perfs = []
      #p statfiles
      while ((statfile = statfiles.pop()) && count<10)
        #p statfile
        perf = calc_avg_performance(statfile, true)
        if (perf==nil || perf.include?(0.0)) # failed experiment
	   #puts "reject " + statfile.to_s()
	   next 
        end
 	count+=1
	perfs.push(perf.avg)
      end
      next if (perfs.empty?())
      length = length.to_i()
      perf = perfs.avg
      output.print "#{proto} #{fastpath} #{p} #{fb} #{length} #{perf/1e6} #{perf*length*8/1e9} #{count} #{perfs.min/1e6} #{perfs.max/1e6} "
      line +=1
      puts "" if (line%2==0) 
   # end
  end
end

if __FILE__==$0
    Dir.chdir("output")
    parse_fp_result("XIA")
    parse_fp_result("IP")
end
