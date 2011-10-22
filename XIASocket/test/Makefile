.PHONY: clean

CFLAGS=-I../API
LDFLAGS =  -lprotobuf -lpthread 

all: pCIDsl

pCIDsl: putCIDslarge.c
	g++ $(CFLAGS) $(LDFLAGS) -o $@ $<  ../API/libXsocket.a

clean:
	rm -f pCIDsl

