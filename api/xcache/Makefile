include ../../xia.mk

.PHONY: all clean

PROTOC=protoc
CFLAGS+=-c -fpic
LDFLAGS+=-lprotobuf $(XLIB)/libXsocket.so

PROTOS		:= $(wildcard *.proto)
PROTO_SRCS	:= $(PROTOS:.proto=.pb.cc)
PROTO_OBJS=$(PROTO_SRCS:.cc=.o)

SOURCES=XcacheApis.c xcache_sock.c
OBJS=$(SOURCES:.c=.o)

LIB=$(XLIB)/libxcache.so

all: $(LIB)

%.pb.cc: %.proto
	$(PROTOC) --cpp_out=. $<

%.pb.o:  %.pb.cc
	$(CC) $(CFLAGS) $< -o $@

%.o: %.c $(INC)/xcache.h
	$(CC) $(CFLAGS) $< -o $@

$(LIB): $(PROTO_OBJS) $(OBJS)
	$(LD) -fPIC --shared -o $@ $(OBJS) $(PROTO_OBJS) $(LDFLAGS)


clean:
	rm -f $(OBJS)
	rm -f *.pb.*
	rm -f $(LIB)
