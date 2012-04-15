CC=g++
LD=g++
CFLAGS=-I../../XIASocket/API -ggdb
LDFLAGS=-lprotobuf -lpthread
SOURCES=ns.cc ../../XIASocket/API/libXsocket.a

all: ns

ns: $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	

clean:
	rm ns
	
	
