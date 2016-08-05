logger: logger.o
	g++ -o logger logger.o -lm -std=c++11

logger.o: logger.cc logger.h
	g++ -c logger.cc -std=c++11

clean:
	rm -f logger 
	rm -f *.o