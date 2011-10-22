.PHONY: clean

CFLAGS=-I../API
LDFLAGS =  -lprotobuf -lpthread

all: xs ps xc pc

xs: Server.c 
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
ps: pingServer.c
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
xc: Client.c                                                 
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
pc: pingClient.c                                             
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

clean:
	rm -f xs ps pc xc

