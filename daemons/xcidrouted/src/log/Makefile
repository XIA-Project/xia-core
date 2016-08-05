test: test.o logger.o
	g++ -o test test.o logger.o -lm -std=c++11

test.o: test.cpp
	g++ -c test.cpp -std=c++11

logger.o: logger.cc logger.h
	g++ -c logger.cc -std=c++11

clean:
	rm -f logger test 
	rm -f *.o
	rm -f log/*.dat