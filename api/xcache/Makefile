include ../../xia.mk

.PHONY: all clean

PROTOC=protoc
CFLAGS+=-c -fpic
LDFLAGS+=-lprotobuf

SOURCES=XcacheApis.c xcache_sock.c
PROTO=xcache_cmd.proto

OBJS=xcache_cmd.pb.o $(SOURCES:.c=.o)
LIB=$(XLIB)/libxcache.so

all: $(LIB) xcache_cmd.pb.cc

%.pb.cc %.pb.h: %.proto
	$(PROTOC) --cpp_out=. $<

%.o: %.c xcache_cmd.pb.h $(INC)/xcache.h
	$(CC) $(CFLAGS) $< -o $@

%.pb.o:  %.pb.cc
	$(CC) $(CFLAGS) $< -o $@


$(LIB): $(OBJS)
	$(LD) --shared -o $@ $(OBJS) $(LDFLAGS)


clean:
	rm -f $(OBJS)
	rm -f *.pb.*
	rm -f $(LIB)
