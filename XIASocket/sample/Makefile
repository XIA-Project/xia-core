.PHONY: clean

CFLAGS=-I../API

all: xs ps xc pc

xs: Server.c 
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a
ps: pingServer.c
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a
xc: Client.c 
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a
pc: pingClient.c
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a

clean:
	rm -f xs ps pc xc
