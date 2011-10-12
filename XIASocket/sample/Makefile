.PHONY: clean

CFLAGS=-I../API
LDFLAGS =  -lprotobuf

all: xs ps xc pc

xs: Server.c 
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< ../API/libXsocket.a
ps: pingServer.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< ../API/libXsocket.a
xc: Client.c 
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< ../API/libXsocket.a
pc: pingClient.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< ../API/libXsocket.a

clean:
	rm -f xs ps pc xc

