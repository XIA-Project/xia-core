include ../../xia.mk
VPATH=../common

.PHONY: all clean

CFLAGS += -I$(APIDIR)/xsocket/minini
LDFLAGS += $(LIBS)

COMMON=csclient.cc XIARouter.cc
SOURCES_CLIENT=xhcp_client.cc xhcp_interface.cc xhcp_beacon.cc
SOURCES_SERVER=xhcp_server.cc xhcp_beacon.cc
CLIENT=$(BINDIR)/xhcp_clientd
SERVER=$(BINDIR)/xhcp_serverd

all: $(CLIENT) $(SERVER)

$(CLIENT): $(SOURCES_CLIENT) $(COMMON)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

$(SERVER): $(SOURCES_SERVER) $(COMMON)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	-rm $(CLIENT) $(SERVER)
