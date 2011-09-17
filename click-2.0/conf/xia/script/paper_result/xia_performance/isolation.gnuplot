#!/usr/bin/env gnuplot 
		
set term pdf size 3.00in, 2.50in
		
set output "isolation.pdf"
set xlabel "Fraction of Resource\n Allocated to Content Principal"
set ylabel "Fraction of Content\n Packets Forwarded"
		
		
set style  line 1 lt 6 lc rgb "red"  linewidth 4  pointtype 1 pointsize 0.4
set style  line 2 lt 4 lc rgb "red"  linewidth 5  pointtype 1 pointsize 0.4
set style  line 3 lt 6 lc rgb "cyan"  linewidth 4  pointtype 2 pointsize 0.4
set style  line 4 lt 4 lc rgb "cyan"  linewidth 5  pointtype 2 pointsize 0.4
set style  line 5 lt 6 lc rgb "blue"  linewidth 4  pointtype 3 pointsize 0.4
set style  line 6 lt 4 lc rgb "blue"  linewidth 5  pointtype 3 pointsize 0.4
set style  line 7 lt 6 lc rgb "grey"  linewidth 4  pointtype 4 pointsize 0.4
set style  line 8 lt 3 lc rgb "grey"  linewidth 5  pointtype 4 pointsize 0.4
set style  line 9 lt 6 lc rgb "green"  linewidth 4  pointtype 5 pointsize 0.4
set style  line 10 lt 4 lc rgb "green"  linewidth 5  pointtype 5 pointsize 0.4
set style  line 11 lt 6 lc rgb "purple"  linewidth 4  pointtype 6 pointsize 0.5
set style  line 12 lt 4 lc rgb "purple"  linewidth 5  pointtype 6 pointsize 0.5
set yrange [0:1]		
set xrange [0:1]		

#set ytics nomirror
set grid
#set key box
#set key  enhanced  bottom right spacing 0.85
		
plot 'isolation' u  ($1/12):4 w lp ls 1 notitle
#     'isolation' u  ($1/12):4 w l ls 8 notitle	
