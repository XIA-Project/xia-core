include ../../../../xia.mk
CFLAGS+=-g -I../../../click/include/click
LDFLAGS+=$(LIBS) -lpthread
all: server client

server:
	$(CC) $(CFLAGS) xftp_basic_server.c -o xftp_basic_server -lprotobuf $(LDFLAGS)

client:
	$(CC) $(CFLAGS) xftp_basic_client.c -o xftp_basic_client -lprotobuf $(LDFLAGS)

clean:
	rm -f chunks_server chunks_client xftp_basic_client xftp_basic_server
