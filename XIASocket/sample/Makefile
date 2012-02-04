.PHONY: clean

CC=g++
CFLAGS=-I../API
LDFLAGS =  -lprotobuf -lpthread

all: ../API/libXsocket.a xs_stream xc_stream xs_dgram xc_dgram \
	ps pc vs ps_beacon pc_beacon echoclient-d echoclient echoserver opttest xcmpping xcmptraceroute

echoclient-d: echoclient-d.c  ../API/libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

echoclient: echoclient.c  ../API/libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

echoserver: echoserver.c  ../API/libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

opttest: opttest.c  ../API/libXsocket.a
	gcc -Wall -Wextra $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

xcmpping: xcmpping.c ../API/libXsocket.a ./XIAResolver/XgetDAGbyname.c
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a  ./XIAResolver/XgetDAGbyname.c $(LDFLAGS) 

xcmptraceroute: xcmptraceroute.c ../API/libXsocket.a ./XIAResolver/XgetDAGbyname.c
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a ./XIAResolver/XgetDAGbyname.c $(LDFLAGS) 

xs_stream: Server_stream.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

xc_stream: Client_stream.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

xs_dgram: Server_dgram.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

xc_dgram: Client_dgram.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

ps: pingServer.c ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

pc: pingClient.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

vs: videoXIAserver.c ../API/libXsocket.a
	g++ $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS)

ps_beacon: pingServer_beacon.c ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS)

pc_beacon: pingClient_beacon.c ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS)

clean:
	rm -f xs_stream ps xc_stream pc vs ps_beacon pc_beacon xs_dgram xc_dgram opttest echoclient xcmpping xcmptraceroute

