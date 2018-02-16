include ../../../xia.mk

.PHONY: all clean

PROTOS := $(wildcard *.proto)
PROTO_SRCS := $(PROTOS:.proto=.pb.cc)

SOURCES=fs.cc fs_client_request.cc fs_thread_pool.cc fs_worker.cc \
		fs_fetch_request.cc fs_irq_table.cc fs_push_request.cc \
		$(PROTO_SRCS)

OBJECTS=$(SOURCES:.cc=.o)
FS=$(BINDIR)/fs

CFLAGS+=-I$(APIDIR)/include -I../../common -L$(XLIB) -Wall
LDFLAGS+=-L$(XLIB) -lXsocket -ldagaddr -lxcache -lprotobuf -Wl,-rpath,$(XLIB)

all: $(PROTO_SRCS) $(FS)

%.pb.cc: %.proto
	protoc -I. --cpp_out=. $<

%.o: %.cc
	$(CC) $(CFLAGS) -g -c -fpic $< -o $@

$(FS): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	-rm -f *.o $(FS)
	rm -f $(PROTO_SRCS) *.pb.cc *.pb.h
