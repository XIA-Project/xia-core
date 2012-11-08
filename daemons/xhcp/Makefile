CC=g++
LD=g++
CFLAGS=-I../../XIASocket/minini -I../../XIASocket/API -ggdb
LDFLAGS=-lprotobuf -lpthread
SOURCES_CLIENT=xhcp_client.cc ../common/csclient.cc ../common/XIARouter.cc ../../XIASocket/API/libXsocket.a
SOURCES_SERVER=xhcp_server.cc ../common/csclient.cc ../common/XIARouter.cc ../../XIASocket/API/libXsocket.a
CLIENT=xhcp_clientd
SERVER=xhcp_serverd

all: $(CLIENT) $(SERVER)

$(CLIENT): $(SOURCES_CLIENT)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_CLIENT) $(LDFLAGS)

$(SERVER): $(SOURCES_SERVER)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_SERVER) $(LDFLAGS)

clean:
	rm $(CLIENT) $(SERVER)
