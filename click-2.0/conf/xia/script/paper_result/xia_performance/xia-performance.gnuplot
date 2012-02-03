#!/usr/bin/env gnuplot 
		
set term pdf size 3.30in, 1.25in
		
set output "forwarding_performance_tput.pdf"
set xlabel "Packet size (bytes)"
set ylabel "Throughput(Gbps)"
		
		
set style  line 1 lt 6 lc rgb "red"  linewidth 4  pointtype 1 pointsize 0.4
set style  line 2 lt 4 lc rgb "red"  linewidth 5  pointtype 1 pointsize 0.4
set style  line 3 lt 6 lc rgb "cyan"  linewidth 4  pointtype 2 pointsize 0.4
set style  line 4 lt 4 lc rgb "cyan"  linewidth 5  pointtype 2 pointsize 0.4
set style  line 5 lt 6 lc rgb "blue"  linewidth 4  pointtype 3 pointsize 0.4
set style  line 6 lt 4 lc rgb "blue"  linewidth 5  pointtype 3 pointsize 0.4
set style  line 7 lt 6 lc rgb "grey"  linewidth 4  pointtype 4 pointsize 0.4
set style  line 8 lt 4 lc rgb "grey"  linewidth 5  pointtype 4 pointsize 0.4
set style  line 9 lt 6 lc rgb "green"  linewidth 4  pointtype 5 pointsize 0.4
set style  line 10 lt 4 lc rgb "green"  linewidth 5  pointtype 5 pointsize 0.4
set style  line 11 lt 6 lc rgb "purple"  linewidth 4  pointtype 6 pointsize 0.5
set style  line 12 lt 4 lc rgb "purple"  linewidth 5  pointtype 6 pointsize 0.5
set yrange [0:30]		

#set ytics nomirror
set grid
set key box
set key  enhanced  bottom right spacing 0.85
		
#plot 'ip' u 2:($3*($2+24)*8/1000) w lp ls 2 title "IP" , \
#     'fb0' u 2:($3*($2+24)*8/1000) w lp ls 4 title "XIA",  \
#     'fb1' u 2:($3*($2+24)*8/1000) w lp ls 6 title "XIA FB1",  \
#     'fb2' u 2:($3*($2+24)*8/1000) w lp ls 8 title "XIA FB2" ,  \
#     'fb3' u 2:($3*($2+24)*8/1000) w lp ls 10 title "XIA FB3"
plot 'ip' u  2:4 w lp ls 2 title "IP" , \
     'fb0' u 2:4 w lp ls 4 title "XIA FB0",  \
     'fb1' u 2:4 w lp ls 6 title "XIA FB1",  \
     'fb2' u 2:4 w lp ls 8 title "XIA FB2" ,  \
     'fb3' u 2:4 w lp ls 10 title "XIA FB3", \
     'via' u 2:4 w lp ls 12 title "XIA VIA" axis x1y1
		
