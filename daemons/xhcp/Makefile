CC=g++
LD=g++

APIDIR=../../api
XINC=$(APIDIR)/include
XLIB=$(APIDIR)/lib
BINDIR=../../bin

CFLAGS=-I$(XINC) -I$(APIDIR)/xsocket/minini -ggdb
LDFLAGS=-lprotobuf -lpthread
SOURCES_CLIENT=xhcp_client.cc ../common/csclient.cc ../common/XIARouter.cc $(XLIB)/libXsocket.so
SOURCES_SERVER=xhcp_server.cc ../common/csclient.cc ../common/XIARouter.cc $(XLIB)/libXsocket.so
CLIENT=$(BINDIR)/xhcp_clientd
SERVER=$(BINDIR)/xhcp_serverd

all: $(CLIENT) $(SERVER)

$(CLIENT): $(SOURCES_CLIENT)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_CLIENT) $(LDFLAGS)

$(SERVER): $(SOURCES_SERVER)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_SERVER) $(LDFLAGS)

clean:
	-rm $(CLIENT) $(SERVER)
