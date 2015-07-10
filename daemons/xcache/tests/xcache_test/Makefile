include ../../../../xia.mk
CC=g++
CFLAGS+=-g -I../../../click/include/click
LDFLAGS+=$(LIBS)
TARGETS=xcache_client xcache_server

all: $(TARGETS)

%: %.c 
	$(CC) $(CFLAGS) $^ ../../api/libxcache.a -o $@ -lprotobuf  $(LDFLAGS)

clean:
	rm -f xcache_test
