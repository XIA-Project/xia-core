CC=g++
LD=g++
CFLAGS=-I../../XIASocket/API -ggdb
LDFLAGS=-lprotobuf -lpthread
SOURCES1=xhcp_client.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a
SOURCES2=xhcp_server.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a

all: xhcp_client xhcp_server

xhcp_client: $(SOURCES1)
	$(CC) -o $@ $(CFLAGS) $(SOURCES1) $(LDFLAGS)
	
xhcp_server: $(SOURCES2)
	$(CC) -o $@ $(CFLAGS) $(SOURCES2) $(LDFLAGS)	

clean:
	rm xhcp_client xhcp_server
	
	
