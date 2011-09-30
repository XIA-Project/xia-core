
all: xia-performance-pps.gnuplot xia-performance.gnuplot isolation.gnuplot isolation xia-fastpath.gnuplot
	./xia-performance-pps.gnuplot
	./xia-performance.gnuplot
	./isolation.gnuplot
	./xia-fastpath.gnuplot
	perl bargraph.pl -pdf  isolation.bargarph > isolation-bar.pdf
