.PHONY: clean

CFLAGS=-I../API
LDFLAGS =  -lprotobuf -lpthread

all: xs ps xc pc vs ../API/libXsocket.a

xs: Server.c 
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
ps: pingServer.c
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
xc: Client.c                                                 
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
pc: pingClient.c                                             
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
vs: videoXIAserver.c
	g++ $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS)

clean:
	rm -f xs ps pc xc vs

