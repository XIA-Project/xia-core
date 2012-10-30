#!/usr/bin/env ruby 
require 'pathname'
require 'tempfile'

class Array
    def sum
      inject(0.0) { |result, el| result + el }
    end
    def mean 
      sum / size
    end
    def avg
	mean
    end
end
class Pathname
  # If this is a directory, returns the most recently modified 
  # file in this directory. Otherwise, return itself. If _matching_
  # is specified, only return files where filename =~ _matching_.
  #
  # Returns nil if no files (or no matching files) are found.
  def most_recent_file(matching=/./, return_array = false)
    return self unless self.directory?
    files = self.entries.collect { |file| self+file }.sort { |file1,file2| file1.mtime <=> file2.mtime }
    files.reject! { |file| ((file.file? and file.to_s =~ matching) ? false : true) }
    if (return_array)
      return files
    else
      return files.last
    end
  end
end

##$proto = ["IP", "XIA"]
#$proto = [ "XIA"]
#$postfix = "FB2-SP"
$proto = ["IP"]
$postfix = "IP-SP-ONLY"

def calc_avg_performance(file, return_array=false)
  tx = []
  file.each_line do |l|
    cols = l.split
    if (cols[0]=="TX") 
	tx.push(cols[2].to_f())
    end
  end 
  #tx.sort! { |x,y| y<=>x }
  if (return_array)
    return tx[20..[tx.size-5, 110].min]
  else
    return tx[20..[tx.size-5, 110].min].avg
  end

end

def parse_result(output_to_file)
  output = $stdout
  pwd = Pathname.getwd() 
  $proto.each do |p|
    if (output_to_file) 
	output = File.new(p, "w")
    end
    file_glob = "#{p}-*-#{$postfix}"
    Pathname.glob(file_glob).sort {|x,y| x.to_s.split('-')[1].to_i() <=>  y.to_s.split('-')[1].to_i()}.each do |f|
      proto, length = f.basename().to_s.split('-')
      statfile = f.most_recent_file(/stat-/)
      perf = calc_avg_performance(statfile)
      length = length.to_i()
      output.puts "#{proto} #{length} #{perf/1e6} #{perf*length*8/1e9}"
    end
    output.close if (output_to_file)
  end
end

def generate_graph()
   #system("bash -c \"gnuplot xia-performance.gnuplot\"")
   system("gnuplot", "xia-performance.gnuplot")
end

if __FILE__==$0
    output_to_file = (ARGV.size>0)
    Dir.chdir("output")
    parse_result(output_to_file)
    if (output_to_file)
      generate_graph();
    end
end
