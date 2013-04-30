
#TARGETS=server client
SOURCES=basic_recv_server native_routing_client scion_paththru_client
CC=g++
CFLAGS=-Wall -Wextra -ggdb -I../../api/include
LIBS=../../api/lib/libXsocket.so ../../api/lib/libdagaddr.so
LDFLAGS=

all: $(SOURCES)

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@

clean:
	rm -rf $(SOURCES)
