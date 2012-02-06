#!/usr/bin/env gnuplot 
		
set term pdf dashed size 3.00in, 2.00in
		
set output "fastpath_performance.pdf"
#set xlabel "XIA Packet types"
set ylabel "Performance (Gbps)"
set xrange [-1.3:3.3]
		
		
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
set yrange [0:25]		

#set ytics nomirror
set grid
set key box
set key  enhanced  bottom right spacing 0.85
		
plot 'ip_fastpath' u  (-1):6:($8*$4*8/1000):($9*$4*8/1000):xticlabel(3) w errorbars ls 5 title "IP Fast Path", \
     'ip_fastpath' u  (-1):15:($17*$4*8/1000):($18*$4*8/1000):xticlabel(3) w errorbars ls 10 title "IP w/o Fast Path", \
     'fastpath' index 0 u  6:xticlabel(3) w lp ls 1 title "XIA Fast Path", \
     'fastpath' index 0 u  0:6:($8*$4*8/1000):($9*$4*8/1000) w errorbars ls 1 notitle , \
     'fastpath' index 0 u  15:xticlabel(3) w lp ls 12 title "XIA w/o Fast Path"  ,\
     'fastpath' index 0 u  0:15:($17*$4*8/1000):($18*$4*8/1000) w errorbars ls 12 notitle

