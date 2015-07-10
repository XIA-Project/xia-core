CC=g++
AR=ar
CFLAGS+=-g -I../../click/include/click
OBJS=XcacheAllocSlice.o XcacheGetChunk.o XcacheInit.o XcacheMain.o XcachePutChunk.o XcachePutFile.o

lib: $(OBJS)
	$(AR) -rcs libxcache.a $(OBJS) ../bin/xcache_cmd.pb.o
	rm -f *.o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^
clean:
	rm -f *.o
	rm -f libxcache.a
