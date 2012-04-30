#!/usr/bin/env gnuplot 
		
set term pdf size 3.00in, 2.00in
		
set output "forwarding_performance_payload.pdf"
set xlabel "Payload size (bytes)"
set ylabel "Goodput (Gbps)"
		
set style  line 1 lt 6 lc rgb "red"  linewidth 3  pointtype 1 pointsize 0.5
set style  line 2 lt 4 lc rgb "red"  linewidth 4  pointtype 1 pointsize 0.5
set style  line 3 lt 6 lc rgb "cyan"  linewidth 3  pointtype 2 pointsize 0.5
set style  line 4 lt 4 lc rgb "cyan"  linewidth 3  pointtype 2 pointsize 0.5
set style  line 5 lt 6 lc rgb "blue"  linewidth 3  pointtype 3 pointsize 0.5
set style  line 6 lt 4 lc rgb "blue"  linewidth 3  pointtype 3 pointsize 0.5
set style  line 7 lt 6 lc rgb "grey"  linewidth 3  pointtype 4 pointsize 0.5
set style  line 8 lt 4 lc rgb "grey"  linewidth 3  pointtype 4 pointsize 0.5
set style  line 9 lt 6 lc rgb "green"  linewidth 3  pointtype 5 pointsize 0.5
set style  line 10 lt 4 lc rgb "green"  linewidth 3  pointtype 5 pointsize 0.5
set style  line 11 lt 6 lc rgb "purple"  linewidth 3  pointtype 6 pointsize 0.5
set style  line 12 lt 4 lc rgb "purple"  linewidth 3  pointtype 6 pointsize 0.5
		
		
set yrange [0:30]		

#set ytics nomirror
set grid
#set nokey
set key box
set key  enhanced  bottom right spacing 0.85
		
#plot 'ip' u 2:($3*($2+24)*8/1000) w lp ls 2 title "IP" , \
#     'fb0' u 2:($3*($2+24)*8/1000) w lp ls 4 title "XIA",  \
#     'fb1' u 2:($3*($2+24)*8/1000) w lp ls 6 title "XIA FB1",  \
#     'fb2' u 2:($3*($2+24)*8/1000) w lp ls 8 title "XIA FB2" ,  \
#     'fb3' u 2:($3*($2+24)*8/1000) w lp ls 10 title "XIA FB3"
plot 'ip' u  ($2-34):($3*8*($2-34)/1000) w lp ls 2 title "IP" , \
     'fb0' u ($2-98+20):($3*8*($2-98+20)/1000) w lp ls 4 title "XIA FB0",  \
     'fb1' u ($2-126+20):($3*8*($2-126+20)/1000) w lp ls 6 title "XIA FB1",  \
     'fb2' u ($2-154+20):($3*8*($2-154+20)/1000) w lp ls 8 title "XIA FB2" ,  \
     'fb3' u ($2-182+20):($3*8*($2-182+20)/1000) w lp ls 10 title "XIA FB3", \
     'via' u ($2-126+20):($3*8*($2-126+20)/1000) w lp ls 12 title "XIA VIA" axis x1y1
		
