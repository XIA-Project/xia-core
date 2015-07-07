include ../../xia.mk
CC=g++
CFLAGS+=-g -I../../click/include/click
LDFLAGS+=$(LIBS)
all: server client

server:
	$(CC) $(CFLAGS) xftp_basic_server.c ../api/libxcache.a -o xftp_basic_server -lprotobuf $(LDFLAGS)

client:
	$(CC) $(CFLAGS) xftp_basic_client.c ../api/libxcache.a -o xftp_basic_client -lprotobuf $(LDFLAGS)

clean:
	rm -f chunks_server chunks_client
