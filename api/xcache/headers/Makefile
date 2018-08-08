include ../../../xia.mk

PROTOS		:= $(wildcard *.proto)
PROTO_SRCS	:= $(PROTOS:.proto=.pb.cc)

.PHONY: all clean

SOURCES=$(PROTO_SRCS) cid_header.cc ncid_header.cc
OBJS=$(SOURCES:.cc=.o)

LIB=$(XLIB)/libxcachehdr.so

LDFLAGS+=-L$(XLIB) -lprotobuf -lcrypto
CFLAGS+=-I$(APIDIR)/include -I$(APIDIR)/xcache -I $(XIADIR)/daemons/common -I../../click/include -I$(APIDIR)/xsocket/minini -L$(XLIB)

%.pb.cc: %.proto
	protoc -I. --cpp_out=. $<

%.o: %.cc
	$(CC) $(CFLAGS) -Wno-unused-parameter -g -c -fpic $< -o $@

$(LIB): $(OBJS)
	$(LD) --shared -g -o $@ $^ -lc -ldl $(LDFLAGS)

all:$(LIB)

clean:
	rm -f $(OBJS) $(LIB) *.pb.*
