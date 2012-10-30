set terminal pdf enhanced dashed size 3in, 2in
#set xlabel 
set yrange [0:2]
set ylabel "Normalized Processing Time"
set xrange [-0.3:3.3]
set output "intra_packet_bench.pdf"
set key box
set key bottom right
set grid 

plot "intra_microbench" index 0 u ($4/$3):xticlabel(2) w l lt 1 lw 5  title "Intra-packet parallelism",\
     '' index 0 u  0:($4/$3):($5/$3):($6/$3) w errorbars  lt 1 lw 6 pointsize 0.5 notitle, \
     ''  u ($4/$3):xticlabel(2)  index 1 w l lt 2 lw 2 lc rgb "black"  title "Serial processing",\
     '' index 1 u  0:($4/$3):($5/$3):($6/$3) w errorbars  lt 1 lw 4 lc rgb "black" pointsize 0.5 notitle
