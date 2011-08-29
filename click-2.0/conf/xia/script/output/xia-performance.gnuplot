#!/usr/bin/env gnuplot 
		
set term pdf size 4.00in, 2.40in
		
set output "forwarding_performance.pdf"
		
set xlabel "Packet size (bytes)"
		
set ylabel "Performance  (Million pps)"
		
set y2label "Performance (Gbps)"
		
		
set style  line 1 lt 6 lc rgb "red"  linewidth 1  pointtype 1 pointsize 1
set style  line 2 lt 1 lc rgb "red"  linewidth 5  pointtype 4 pointsize 0.2
set style  line 3 lt 6 lc rgb "cyan"  linewidth 1  pointtype 1 pointsize 1
set style  line 4 lt 1 lc rgb "cyan"  linewidth 5  pointtype 4 pointsize 0.2
		
set y2tics
set ytics nomirror
set key box
set key at graph 0.99,0.65
		
plot 'IP' u 2:3 w lp ls 1 title "IP (Mpps)" axis x1y1, 'IP' u 2:4 w lp ls 2 title "IP (Gbps)" axis x1y2,  'XIA' u 2:3 w lp ls 3 title "XIA (Mpps)" axis x1y1, 'XIA' u 2:4 w lp ls 4 title "XIA (Gbps)" axis x1y2
		
