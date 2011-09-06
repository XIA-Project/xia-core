#!/usr/bin/env gnuplot 
		
set term pdf size 4.00in, 2.40in
		
set output "relay.pdf"
		
set xlabel "Number of cores"
		
set ylabel "Performance  (Million pps)"
		
set y2label "Performance (Gbps)"
		
		
set style  line 1 lt 6 lc rgb "red"  linewidth 1  pointtype 1 pointsize 1
set style  line 2 lt 1 lc rgb "red"  linewidth 5  pointtype 4 pointsize 0.2
set style  line 3 lt 6 lc rgb "cyan"  linewidth 1  pointtype 1 pointsize 1
set style  line 4 lt 1 lc rgb "cyan"  linewidth 5  pointtype 4 pointsize 0.2
		
set y2tics
set ytics nomirror
set key box
#set key at graph 0.99,0.65
set y2range [0:12.8]

plot  './relay_result' u ($1):($2*64*8/1e9) w lp ls 2 title "Actual performance" axis x1y2, './relay_result' u 1:($1*$3) title "linear scaling" ls 4 w lp
		
