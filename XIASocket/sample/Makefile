.PHONY: clean

CFLAGS=-I../API
LDFLAGS =  -lprotobuf -lpthread

all: xs_stream ps xc_stream pc vs ps_beacon pc_beacon xs_dgram xc_dgram ../API/libXsocket.a

opttest: opttest.c  ../API/libXsocket.a
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a $(LDFLAGS) 

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
	rm -f xs_stream ps xc_stream pc vs ps_beacon pc_beacon xs_dgram xc_dgram

