#!/usr/bin/env gnuplot

set terminal pdf enhanced dashed size 3,2
set output "migration.pdf"

set ytics nomirror
set xtics nomirror

set noy2tics
set nomy2tics

set xlabel "Elapsed time (milliseconds)"
set ylabel "Sequence # (K)"

set nokey

################## begin

set xrange [0.000000:707.914000]
offset = 12293.938500
freeze = 176.978500
server_update = 517.666500
client_update = 530.935500

################## end

ymin = 12.1
ymax = 13.1
rtt = 25 + 2	# the actual RTT was somewat longer than intended (25 ms)

##################

set yrange [ymin:ymax]

set object 1 rect from 0,(12.845-0.0125) to 707.914,12.845 lw 0 fc rgb "gold" 
set style line 1000 linewidth 3 linetype 1 linecolor rgb "grey" 
set arrow from 0,(12.845-0.0125) to 707.914,(12.845-0.0125) nohead linestyle 1000
set arrow from 0,12.845 to 707.914,12.845 nohead linestyle 1000

# vertical lines
set style line 1000 linewidth 4 linetype 2 linecolor rgb "#ff0000" 
set arrow from freeze,ymin to freeze,ymax nohead linestyle 1000

set style line 1001 linewidth 4 linetype 3 linecolor rgb "#6a5acd"
set arrow from server_update,ymin to server_update,ymax nohead linestyle 1001

set style line 1002 linewidth 4 linetype 1 linecolor rgb "#00ff00"
set arrow from client_update,ymin to client_update,ymax nohead linestyle 1002

# horizontal labels
set style line 100 linewidth 2 linetype 1 linecolor rgb "#000000" 
set label 'Service frozen' font "Helvetica Bold,5" center at (freeze + server_update) / 2, (ymin + (ymax - ymin) * 0.88)
set label '(341 ms)' font "Helvetica Bold,5" center at (freeze + server_update) / 2, (ymin + (ymax - ymin) * 0.78)		# XXX: HARDCODED
set arrow from freeze, (ymin + (ymax - ymin) * 0.83) to server_update, (ymin + (ymax - ymin) * 0.83) heads linestyle 100

set style line 101 linewidth 2 linetype 1 linecolor rgb "#000000" 
set label 'Service rebind' font "Helvetica Bold,5" right at server_update - 70, (ymin + (ymax - ymin) * 0.20)
set arrow from server_update - 60, (ymin + (ymax - ymin) * 0.20) to server_update, (ymin + (ymax - ymin) * 0.20) head linestyle 101

set style line 102 linewidth 2 linetype 1 linecolor rgb "#000000" 
set label 'Client rebind' font "Helvetica Bold,5" right at server_update - 70, (ymin + (ymax - ymin) * 0.10)
set arrow from server_update - 60, (ymin + (ymax - ymin) * 0.10) to client_update, (ymin + (ymax - ymin) * 0.10) head linestyle 102

set style arrow 8 heads size screen 0.008,90 ls 2
set style line 10000 linewidth 5 linetype 1 linecolor rgb "#000000"
# left RTT
set arrow from freeze, (ymin + (ymax - ymin) * 0.30) to freeze + rtt/2, (ymin + (ymax - ymin) * 0.30) heads arrowstyle 8 linestyle 10000
set label ' 0.5 RTT' font "Helvetica,5" left at freeze + 20, (ymin + (ymax - ymin) * 0.30)
#set label ' (in-flight packets)' font "Helvetica,5" left at freeze, (ymin + (ymax - ymin) * 0.17)
# right RTT
set arrow from client_update, (ymin + (ymax - ymin) * 0.68) to client_update + rtt, (ymin + (ymax - ymin) * 0.68) heads arrowstyle 8 linestyle 10000
set label ' 1 RTT' font "Helvetica,5" left at client_update + rtt, (ymin + (ymax - ymin) * 0.68)
set arrow from server_update, (ymin + (ymax - ymin) * 0.60) to server_update + rtt/2, (ymin + (ymax - ymin) * 0.60) heads arrowstyle 8 linestyle 10000
set label ' 0.5 RTT' font "Helvetica,5" left at server_update + rtt/2, (ymin + (ymax - ymin) * 0.60)
#set label '(27 ms)' font "Helvetica,5" left at client_update + rtt, (ymin + (ymax - ymin) * 0.14)		# XXX: HARDCODED

set label 'Req sent' font ",5" left at 10, (ymin + (ymax - ymin) * 0.45) textcolor rgb "blue"
set label 'Resp recv' font ",5" left at 10, (ymin + (ymax - ymin) * 0.15) textcolor rgb "violet"

plot "plot.dat" \
            index 1 using ($1 * 1000 - offset):($2 / 1000) axes x1y1 with points ls 7 ps 0.20 lt 3 title "Sent by client", \
     ""     index 4 using ($1 * 1000 - offset):($2 / 1000) axes x1y1 with points ls 7 ps 0.20 lt 4 title "Received by client"

