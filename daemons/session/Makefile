include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
LDFLAGS +=-lprotobuf -lc -ldl
SOURCES=sessiond.cpp session.pb.cc utils.cpp
NS_SOURCES=ns.cpp session.pb.cc
#SESSIOND=$(BINDIR)/sessiond
SESSIOND=sessiond
NS=ns

all: session.pb.cc $(SESSIOND) $(NS)

$(SESSIOND): $(SOURCES) xia.cpp ip.cpp
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	

$(NS): $(NS_SOURCES)
	$(CC) -o $@ $(CFLAGS) $(NS_SOURCES) $(LDFLAGS)	

session.pb.cc: session.proto
	protoc --cpp_out=. session.proto

clean:
	-rm $(SESSIOND)
	-rm -f *.o session.pb.*
