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
  def most_recent_file(matching=/./)
    return self unless self.directory?
    files = self.entries.collect { |file| self+file }.sort { |file1,file2| file1.mtime <=> file2.mtime }
    files.reject! { |file| ((file.file? and file.to_s =~ matching) ? false : true) }
    files.last
  end
end

$proto = ["IP", "XIA"]
def calc_avg_performance(file)
  tx = []
  file.each_line do |l|
    cols = l.split
    if (cols[0]=="TX") 
	tx.push(cols[2].to_f())
    end
  end 
  #tx.sort! { |x,y| y<=>x }
  return tx[20..[tx.size-1, 50].min].avg
end

def parse_result(output_to_file)
  output = $stdout
  pwd = Pathname.getwd() 
  $proto.each do |p|
    if (output_to_file) 
	output = File.new(p, "w")
    end
    Pathname.glob("#{p}-*").sort {|x,y| x.to_s.split('-')[1].to_i() <=>  y.to_s.split('-')[1].to_i()}.each do |f|
      proto, length = f.basename().to_s.split('-')
      statfile = f.most_recent_file(/stat-/)
      perf = calc_avg_performance(statfile)
      length = length.to_i()
      output.puts "#{proto} #{length} #{perf/1e6} #{perf*length*8/1e9}"
    end
  end
end

def generate_graph()
  #$proto.each do |p|
     #file = Tempfile.new('xia-exp')
     file = File.new('xia-performance.gnuplot', 'w')
     file.puts("set term pdf size 5.00in, 3.00in
		\nset output \"forwarding_performance.pdf\"
		\nset xlabel \"Packet size (bytes)\"
		\nset ylabel \"Performance  (Million pps)\"
		\nset y2label \"Performance (Gbis)\"
		")
     
     file.puts("set y2tics
		\nset ytics nomirror
		\nplot 'IP' u 2:3 w lp lw 2 title \"IP forwarding (Mpps)\" axis x1y1, 'IP' u 2:4 w lp lw 3 title \"IP forwarding (Gbps)\" axis x1y2,  'XIA' u 2:3 w lp lw 2 title \"XIA forwarding (Mpps)\" axis x1y1, 'XIA' u 2:4 w lp lw 3 title \"XIA forwarding (Gbps)\" axis x1y2
		")
     file.close
     p file.path
     p "gnuplot #{file.path}"
     #file.unlink
  #end
end

if __FILE__==$0
    output_to_file = (ARGV.size)
    Dir.chdir("output")
    parse_result(output_to_file)
    if (output_to_file)
      generate_graph();
    end
end
