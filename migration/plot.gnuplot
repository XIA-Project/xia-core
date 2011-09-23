#!/usr/bin/env gnuplot

set terminal pdf enhanced
set output "plot.pdf"

#set y2tics ("HID_A" 1, "HID_B" 2)
#set nomy2tics

set xlabel "Elapsed time (seconds)"
set ylabel "Transfer rate (pps)"

set key left bottom

plot "plot.dat" index 0 using 1:($2) axes x1y1 with lines linewidth 4 notitle, \
     ""		index 1 using 1:($2) axes x1y2 with points ls 19 ps 0.1 lt 3 notitle, \
     ""         index 2 using 1:($2) axes x1y2 with points ls 19 ps 0.1 lt 3 notitle, \
     ""         index 5 using 1:($2) axes x1y2 with points ls 19 ps 0.1 lt 4 notitle, \
     ""         index 6 using 1:($2) axes x1y2 with points ls 19 ps 0.1 lt 4 notitle, \
     ""		index 3 using 1:($3 + 1) axes x1y2 with points ls 19 ps 0.1 lt 3 notitle, \
     ""         index 4 using 1:($3 + 1) axes x1y2 with points ls 19 ps 0.1 lt 3 notitle, \
     ""         index 7 using 1:($3 + 1) axes x1y2 with points ls 19 ps 0.1 lt 4 notitle, \
     ""         index 8 using 1:($3 + 1) axes x1y2 with points ls 19 ps 0.1 lt 4 notitle

