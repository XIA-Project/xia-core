.PHONY: clean

CFLAGS=-I../API

all: xc pc

xc: Client.c 
	gcc $(CFLAGS) -lprotobuf -o $@ $< ../API/libXsocket.a
pc: pingClient.c
	gcc $(CFLAGS) -lprotobuf -o $@ $< ../API/libXsocket.a

clean:
	rm -f xs ps pc xc
