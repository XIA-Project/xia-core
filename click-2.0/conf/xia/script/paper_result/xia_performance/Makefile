
all: xia-performance-pps.gnuplot xia-performance.gnuplot isolation.gnuplot isolation
	./xia-performance-pps.gnuplot
	./xia-performance.gnuplot
	./isolation.gnuplot
	perl bargraph.pl -pdf  isolation.bargarph > isloation-bar.pdf
