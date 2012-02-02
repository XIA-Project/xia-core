
all: xia-performance-pps.gnuplot xia-performance.gnuplot isolation.gnuplot isolation xia-fastpath.gnuplot
	./xia-performance-pps.gnuplot
	./xia-performance.gnuplot
	./xia-performance-goodput.gnuplot
	./isolation.gnuplot
	./xia-fastpath.gnuplot
	./xia-fastpath-pps.gnuplot
	perl bargraph.pl -pdf  isolation.bargarph > isolation-bar.pdf
