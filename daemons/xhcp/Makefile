CC=g++
LD=g++
CFLAGS=-I../../XIASocket/API -ggdb
LDFLAGS=-lprotobuf -lpthread
SOURCES_CLIENT_H0=xhcp_client.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a
SOURCES_CLIENT_H1=xhcp_client_h1.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a
SOURCES_SERVER=xhcp_server.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a

all: xhcp_client xhcp_server xhcp_client_h0 xhcp_server_r0 xhcp_client_h1 xhcp_server_r1

xhcp_client: $(SOURCES_CLIENT_H0)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_CLIENT_H0) $(LDFLAGS)
	
xhcp_server: $(SOURCES_SERVER)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_SERVER) $(LDFLAGS)
	
	
xhcp_client_h0: $(SOURCES_CLIENT_H0)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_CLIENT_H0) $(LDFLAGS)
	
xhcp_server_r0: $(SOURCES_SERVER)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_SERVER) $(LDFLAGS)	
	
xhcp_client_h1: $(SOURCES_CLIENT_H1)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_CLIENT_H1) $(LDFLAGS)
	
xhcp_server_r1: $(SOURCES_SERVER)
	$(CC) -o $@ $(CFLAGS) $(SOURCES_SERVER) $(LDFLAGS)		

clean:
	rm xhcp_client xhcp_server xhcp_client_h0 xhcp_server_r0 xhcp_client_h1 xhcp_server_r1
	
	
