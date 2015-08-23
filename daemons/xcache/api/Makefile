CC=g++
AR=ar
CFLAGS+=-g -I../../click/include/click -shared
OBJS=XcacheApis.o ../common/xcache_sock.o

lib: $(OBJS)
	$(AR) -rcs libxcache.a $(OBJS) ../bin/xcache_cmd.pb.o
	rm -f *.o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^
clean:
	rm -f *.o
	rm -f libxcache.a
