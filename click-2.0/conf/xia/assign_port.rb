
ARGF.each_line do |l|
  prefix, _ = l.split
  port = rand(4)+1  
  puts "#{prefix} #{port}"
end
