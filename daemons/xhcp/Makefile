CC=g++
LD=g++
CFLAGS=-I../../XIASocket/minini -I../../XIASocket/API -ggdb
LDFLAGS=-lprotobuf -lpthread
SOURCES_CLIENT=xhcp_client.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a
SOURCES_SERVER=xhcp_server.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a

all: xhcp_client xhcp_server

xhcp_client: $(SOURCES_CLIENT)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_CLIENT) $(LDFLAGS)
	
xhcp_server: $(SOURCES_SERVER)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_SERVER) $(LDFLAGS)
	
clean:
	rm xhcp_client xhcp_server
