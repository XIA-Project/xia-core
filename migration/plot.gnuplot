#!/usr/bin/env gnuplot

set terminal pdf enhanced dashed size 3,2.5
set output "migration.pdf"

set ytics nomirror

set y2tics autofreq
set my2tics

set xlabel "Elapsed time (milliseconds)"
set ylabel "Processing rate (qps)"
set y2label "Sequence number (K)"

set key outside center bottom box

set xrange [0:2000]
set yrange [0:1100]
set y2range [20:24]

plot "plot.dat" \
            index 0 using ($1 * 1000 - 11000):($2) axes x1y1 with lines linewidth 8 lt 1 title "250 ms moving average", \
     ""     index 3 using ($1 * 1000 - 11000):($3 / 1000 + 0 + 10) axes x1y2 with points ls 7 ps 0.2 lt 3 title "Queries processed at HID_S", \
     ""     index 4 using ($1 * 1000 - 11000):($3 / 1000 + 0 + 10) axes x1y2 with points ls 7 ps 0.2 lt 4 title "Queries processed at HID_{NewServer}", \
     ""     index 9 using ($1 * 1000 - 11000):($2) axes x1y1 with lines linewidth 2 lt 2 lc rgb "#ff00ff" title "Service frozen (final iteration)", \
     ""     index 11 using ($1 * 1000 - 11000):($2) axes x1y1 with lines linewidth 2 lt 3 lc rgb "#6a5acd" title "Service resumed \\& rebound", \
     ""     index 10 using ($1 * 1000 - 11000):($2) axes x1y1 with lines linewidth 2 lt 1 lc rgb "#00ff00" title "Client rebound"

