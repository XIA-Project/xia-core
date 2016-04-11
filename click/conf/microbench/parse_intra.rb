#!/usr/bin/env ruby 
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

def parse_serial(f)
  newlinecnt = 0   # consequtive new line count  
  item = []
  File.new(f).each_line do |l|
    next if !(l=~/^name /) 
    _, name, _, _, _, _, _, avg = l.split 
    item.push(avg.to_f)
  end
  return item
end

def parse_intra(f)
  newlinecnt = 0   # consequtive new line count  
  data = []
  item = []
  File.new(f).each_line do |l|
    if (l =~ /^\s*$/)
      newlinecnt+=1
    else
      newlinecnt = 0 
    end
    if (newlinecnt==2)
      newlinecnt = 0   # this is a new set of data
      if (!item.empty?())
        data.push(item)
        item = []
      end
    end
    next if !(l=~/^name Intra/) 
    _, name, _, _, _, _, _, avg = l.split 
    m = /Intra@5\/TimestampAccum@(\d+)/.match(name)
    timestampacuum_number = m[1].to_i()
    next if (timestampacuum_number>46)
    item.push(avg.to_f)
  end
  if (!item.empty?())
    data.push(item)
    item = []
  end 

  slowest_path = []
  data.each do |item|
    slowest_path.push(item.max) 
  end
  return slowest_path
end

files = (0..3).to_a.map {|i| "intra" + i.to_s() }

norm = []
files.each do |f|
   time = parse_intra(f)
   norm.push(time.avg) 
   puts f + " "+time.avg.to_s() + " " + time.max.to_s()+ " " + time.min.to_s()
end

puts norm.map {|x| x/norm[0] }.join(" ")


files = (0..3).to_a.map {|i| "serial" + i.to_s() }
norm = []
files.each do |f|
   time = parse_serial(f)
   puts f + " "+time.avg.to_s() + " " + time.max.to_s()+ " " + time.min.to_s()
end
