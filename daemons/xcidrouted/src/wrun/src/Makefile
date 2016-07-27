include ../../../../../xia.mk

.PHONY: clean

LDFLAGS+=$(LIBS) -lprotobuf -lpthread -lm
TARGETS= server client

all: $(TARGETS)

parser.o: parser.cc parser.h
	$(CC) $(CFLAGS) -c parser.cc -std=c++11

client.o: client.cc parser.h
	$(CC) $(CFLAGS) -c client.cc -std=c++11

server.o: server.cc parser.h
	$(CC) $(CFLAGS) -c server.cc -std=c++11

client: client.o parser.o
	g++ -o client client.o parser.o $(LDFLAGS) -std=c++11

server: server.o parser.o
	g++ -o server server.o parser.o $(LDFLAGS) -std=c++11

clean:
	rm -f client server
	rm -f *.o
	rm -f *.dat