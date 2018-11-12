include ../../../xia.mk

.PHONY: all clean

PROTOS := $(wildcard *.proto)
PROTO_SRCS := $(PROTOS:.proto=.pb.cc)

SOURCES=icid_monitor.cc icid_thread_pool.cc \
		icid_receive_request.cc icid_irq_table.cc icid_push_request.cc \
		$(PROTO_SRCS)

OBJECTS=$(SOURCES:.cc=.o)
ICIDLIB=$(XLIB)/libicid.so

CFLAGS+=-I$(APIDIR)/include -I$(APIDIR)/xcache -I../ -I$(APIDIR)/xsocket/minini -I../../common -I../../../click/include -L$(XLIB) -Wall
LDFLAGS+=-L$(XLIB) -lXsocket -ldagaddr -lxcache -lprotobuf -Wl,-rpath,$(XLIB)

all: $(PROTO_SRCS) $(ICIDLIB)

%.pb.cc: %.proto
	protoc -I. --cpp_out=. $<

%.o: %.cc
	$(CC) $(CFLAGS) -g -c -fpic $< -o $@

$(ICIDLIB): $(OBJECTS)
	$(LD) -fPIC --shared -o $@ $^ $(LDFLAGS)

clean:
	-rm -f *.o $(ICIDLIB)
	rm -f $(PROTO_SRCS) *.pb.cc *.pb.h
