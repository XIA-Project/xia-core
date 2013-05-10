include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
LDFLAGS +=-lprotobuf -lc -ldl
SOURCES=sessiond.cpp session.pb.cc
#SESSIOND=$(BINDIR)/sessiond
SESSIOND=sessiond

all: session.pb.cc $(SESSIOND)

$(SESSIOND): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	

session.pb.cc: session.proto
	protoc --cpp_out=. session.proto

#session.pb.o:
	#$(CC) $(CFLAGS) session.pb.cc  -o $@

clean:
	-rm $(SESSIOND)
	-rm -f *.o session.pb.*
