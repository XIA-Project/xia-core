#!/usr/bin/env ruby
cnt =1
if (ARGV.size!=1)
    puts "Specify one filename to process"
    exit
end
input= ARGV[0]
output= File.basename(ARGV[0], File.extname(ARGV[0]))+".click"

if (File.exists?(output))
    print  "#{output} already exists, do you want to overwrite? (y/N)"
    response = $stdin.gets.chomp
    if (response!='y')
        puts "Exiting without rewriting"
    end
end

output = File.open(output,"w")

File.open(input).each_line do |line|
  if (line =~/include/)
      cols= line.split
      if (cols.size()!=2)
          puts "Illegal use of include in line "+ cnt.to_s
          exit
      end
      filename =cols[1]
      File.open(filename).each_line do |source|
         output.puts source
      end
  else
      output.puts line
  end
  cnt+=1
end

output.close()
