AR=ar
CFLAGS+=-g -I../../click/include/click -shared
OBJS=XcacheApis.o ../common/xcache_sock.o ../proto/xcache_cmd.pb.o

.PHONY: all

LIBXCACHE= libxcache.a

$(LIBXCACHE): $(OBJS)
	$(AR) -rcs libxcache.a $(OBJS)

all: $(LIBXCACHE)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm -f $(OBJS)
	rm -f libxcache.a
