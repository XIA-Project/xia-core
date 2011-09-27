
ARGF.each_line do |l|
  prefix, _ = l.split
  port = rand(3) +1
  puts "#{prefix} #{port}"
end
