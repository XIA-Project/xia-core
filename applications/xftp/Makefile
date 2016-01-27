include ../../xia.mk
CFLAGS+=-g -I../../../click/include/click
LDFLAGS+=$(LIBS) -lpthread -lprotobuf
all: server client

server:
	$(CC) $(CFLAGS) xftp_basic_server.c -o xftp_basic_server $(LDFLAGS)

client:
	$(CC) $(CFLAGS) xftp_basic_client.c -o xftp_basic_client $(LDFLAGS)

clean:
	rm -f chunks_server chunks_client xftp_basic_client xftp_basic_server
