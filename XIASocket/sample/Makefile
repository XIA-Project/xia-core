.PHONY: clean

CFLAGS=-I../API
LDFLAGS =  -lprotobuf -lpthread

all: xs ps xc pc vs ../API/libXsocket.a

xs: Server.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
ps: pingServer.c ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
xc: Client.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
pc: pingClient.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 
vs: videoXIAserver.c ../API/libXsocket.a
	g++ $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS)

clean:
	rm -f xs ps pc xc vs

