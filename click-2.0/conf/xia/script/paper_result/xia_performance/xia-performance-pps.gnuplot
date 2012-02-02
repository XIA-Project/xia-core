#!/usr/bin/env gnuplot 
		
set term pdf size 3.30in, 1.25in
		
set output "forwarding_performance-pps.pdf"
set xlabel "Packet size (bytes)"
set ylabel "Throughput(Mpps)"
		
		
set style  line 1 lt 6 lc rgb "red"  linewidth 4  pointtype 1 pointsize 0.5
set style  line 2 lt 4 lc rgb "red"  linewidth 5  pointtype 1 pointsize 0.5
set style  line 3 lt 6 lc rgb "cyan"  linewidth 4  pointtype 2 pointsize 0.5
set style  line 4 lt 4 lc rgb "cyan"  linewidth 5  pointtype 2 pointsize 0.5
set style  line 5 lt 6 lc rgb "blue"  linewidth 4  pointtype 3 pointsize 0.5
set style  line 6 lt 4 lc rgb "blue"  linewidth 5  pointtype 3 pointsize 0.5
set style  line 7 lt 6 lc rgb "slategrey"  linewidth 4  pointtype 4 pointsize 0.5
set style  line 8 lt 4 lc rgb "slategrey"  linewidth 5  pointtype 4 pointsize 0.5
set style  line 9 lt 6 lc rgb "green"  linewidth 4  pointtype 5 pointsize 0.5
set style  line 10 lt 4 lc rgb "green"  linewidth 5  pointtype 5 pointsize 0.5
set style  line 11 lt 6 lc rgb "purple"  linewidth 4  pointtype 6 pointsize 0.5
set style  line 12 lt 4 lc rgb "purple"  linewidth 5  pointtype 6 pointsize 0.5
		

set yrange [0:25]		

set grid
set key box
set key enhanced top right spacing 0.85
		
plot 'ip' u 2:3 w lp ls 1 title "IP" axis x1y1, \
     'fb0' u 2:3 w lp ls 3 title "XIA FB0" axis x1y1,  \
     'fb1' u 2:3 w lp ls 5 title "XIA FB1" axis x1y1,  \
     'fb2' u 2:3 w lp ls 7 title "XIA FB2" axis x1y1,  \
     'fb3' u 2:3 w lp ls 9 title "XIA FB3" axis x1y1, \
     'via' u 2:3 w lp ls 11 title "XIA VIA" axis x1y1
		
