include ../../../xia.mk

.PHONY: clean

TARGETS = nameserver

all: $(TARGETS)

dagurl.o: dagurl.cpp
	$(CC) $(CFLAGS) -c dagurl.cpp 

nameserver.o: nameserver.cpp ../src/utils.h
	$(CC) $(CFLAGS) -c nameserver.cpp 

manifest_test.o: manifest_test.cpp
	$(CC) $(CFLAGS) -I/usr/include/libxml2 -c manifest_test.cpp

nameserver: nameserver.o ../src/utils.o $(LDFLAGS) $(LIBS) $(LIBFLAGS)

dagurl: dagurl.o $(LDFLAGS) $(LIBS) $(LIBFLAGS)

manifest_test: ../src/manifest.o ../src/utils.o manifest_test.o $(LDFLAGS) -lxml2

clean:
	-rm -f dagurl
	-rm -f manifest_test
	-rm -f nameserver
	-rm -f *.o