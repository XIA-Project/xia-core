XIACIDWGen: main.o config.o workload.o
	g++ -o XIACIDWGen main.o config.o workload.o -lm -std=c++11

config.o: config.cc config.h
	g++ -c config.cc -std=c++11

workload.o: workload.cc workload.h
	g++ -c workload.cc -std=c++11

test.o: test.cc
	g++ test.cc -o test -std=c++11

main.o: main.cc
	g++  -c main.cc -std=c++11

clean:
	rm -f XIACIDWGen 
	rm -f *.o
	rm -f *.dat