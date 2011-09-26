#!/usr/bin/env gnuplot

set terminal pdf enhanced dashed size 3,2.5
set output "migration.pdf"

set ytics nomirror

#set y2tics autofreq
#set my2tics
set noy2tics
set nomy2tics

set xlabel "Elapsed time (milliseconds)"
#set ylabel "Processing rate (qps)"
set ylabel "Sequence # (K)"

set key outside center bottom box
#set key outside center bottom

################## begin

set xrange [0.000000:707.914000]
offset = 12293.938500
freeze = 176.978500
server_update = 517.666500
client_update = 530.935500


################## end

ymin = 12
ymax = 13
rtt = 25 + 2	# the actual RTT was somewat longer than intended (25 ms)

##################

set yrange [ymin:ymax]

# vertical lines
set style line 1000 linewidth 4 linetype 2 linecolor rgb "#ff0000" 
set arrow from freeze,ymin to freeze,ymax nohead linestyle 1000

set style line 1001 linewidth 4 linetype 3 linecolor rgb "#6a5acd"
set arrow from server_update,ymin to server_update,ymax nohead linestyle 1001

set style line 1002 linewidth 4 linetype 1 linecolor rgb "#00ff00"
set arrow from client_update,ymin to client_update,ymax nohead linestyle 1002

# horizontal labels
set style line 100 linewidth 2 linetype 1 linecolor rgb "#000000" 
set label 'Service frozen' center at (freeze + server_update) / 2, (ymin + (ymax - ymin) * 0.90)
set label '(341 ms)' center at (freeze + server_update) / 2, (ymin + (ymax - ymin) * 0.80)		# XXX: HARDCODED
set arrow from freeze, (ymin + (ymax - ymin) * 0.85) to server_update, (ymin + (ymax - ymin) * 0.85) heads linestyle 100

set style line 101 linewidth 2 linetype 1 linecolor rgb "#000000" 
set label 'Service rebind' right at server_update - 60, (ymin + (ymax - ymin) * 0.70)
set arrow from server_update - 60, (ymin + (ymax - ymin) * 0.70) to server_update, (ymin + (ymax - ymin) * 0.70) head linestyle 101

set style line 102 linewidth 2 linetype 1 linecolor rgb "#000000" 
set label 'Client rebind' right at server_update - 60, (ymin + (ymax - ymin) * 0.55)
set arrow from server_update - 60, (ymin + (ymax - ymin) * 0.55) to client_update, (ymin + (ymax - ymin) * 0.55) head linestyle 102

set style arrow 8 heads size screen 0.008,90 ls 2
set style line 10000 linewidth 3 linetype 1 linecolor rgb "#000000"
# left RTT
set arrow from freeze, (ymin + (ymax - ymin) * 0.35) to freeze + rtt/2, (ymin + (ymax - ymin) * 0.35) heads arrowstyle 8 linestyle 10000
set label ' 0.5 RTT' left at freeze, (ymin + (ymax - ymin) * 0.28)
set label ' (in-flight packets)' left at freeze, (ymin + (ymax - ymin) * 0.20)
# right RTT
set arrow from server_update, (ymin + (ymax - ymin) * 0.37) to server_update + rtt, (ymin + (ymax - ymin) * 0.37) heads arrowstyle 8 linestyle 10000
set arrow from client_update, (ymin + (ymax - ymin) * 0.32) to client_update + rtt, (ymin + (ymax - ymin) * 0.32) heads arrowstyle 8 linestyle 10000
set label ' 1 RTT' left at client_update + rtt, (ymin + (ymax - ymin) * 0.35)
set label '(27 ms)' left at client_update + rtt, (ymin + (ymax - ymin) * 0.27)

plot "plot.dat" \
            index 3 using ($1 * 1000 - offset):($3 / 1000) axes x1y1 with points ls 7 ps 0.1 lt 3 title "Response sent by service", \
     ""     index 4 using ($1 * 1000 - offset):($3 / 1000) axes x1y1 with points ls 7 ps 0.1 lt 4 title "Response received by client"

#            index 0 using ($1 * 1000 - 14500):($2) axes x1y1 with lines linewidth 8 lt 1 title "250 ms moving average", \

