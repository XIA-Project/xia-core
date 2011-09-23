#!/usr/bin/env gnuplot

set terminal pdf enhanced dashed size 7,3.5
set output "plot.pdf"

set ytics nomirror

set y2tics autofreq
set my2tics

set xlabel "Elapsed time (milliseconds)"
set ylabel "Transfer rate (pps)"
set y2label "Sequence number (K)"

set key box left bottom

set xrange [0:2000]
set yrange [0:1100]
set y2range [0:40]

plot "plot.dat" \
                index 7 using ($1 * 1000 - 11000):($3 / 1000 + 1 + 10) axes x1y2 with points ls 7 ps 0.3 lt 3 title "Server reception", \
     ""         index 8 using ($1 * 1000 - 11000):($3 / 1000 + 1 + 10) axes x1y2 with points ls 7 ps 0.3 lt 3 notitle, \
     ""		index 3 using ($1 * 1000 - 11000):($3 / 1000 + 0 + 10) axes x1y2 with points ls 7 ps 0.3 lt 4 title "Client reception", \
     ""         index 4 using ($1 * 1000 - 11000):($3 / 1000 + 0 + 10) axes x1y2 with points ls 7 ps 0.3 lt 4 notitle, \
     ""		index 0 using ($1 * 1000 - 11000):($2) axes x1y1 with lines linewidth 8 lt 1 title "Client reception rate ", \
     ""		index 9 using ($1 * 1000 - 11000):($2) axes x1y1 with lines linewidth 2 lt 2 lc "black" title "VM frozen", \
     ""		index 11 using ($1 * 1000 - 11000):($2) axes x1y1 with lines linewidth 2 lt 3 lc "black" title "VM resume & server rebind", \
     ""         index 10 using ($1 * 1000 - 11000):($2) axes x1y1 with lines linewidth 2 lt 1 lc "black" title "Client rebind"

#     		index 1 using ($1 * 1000 - 11000):($2 + 3000 + 10000) axes x1y2 with points ls 7 ps 0.3 lt 7 title "Client Ping CSeq", \
#     ""         index 2 using ($1 * 1000 - 11000):($2 + 3000 + 10000) axes x1y2 with points ls 7 ps 0.3 lt 7 notitle, \
#     ""         index 5 using ($1 * 1000 - 11000):($2 + 2000 + 10000) axes x1y2 with points ls 7 ps 0.3 lt 5 title "Server Ping CSeq", \
#     ""         index 6 using ($1 * 1000 - 11000):($2 + 2000 + 10000) axes x1y2 with points ls 7 ps 0.3 lt 5 notitle, \
