include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
LDFLAGS +=-lprotobuf -lc -ldl
SOURCES=sessiond.cpp session.pb.cc utils.cpp
NS_SOURCES=ns.cpp session.pb.cc
#SESSIOND=$(BINDIR)/sessiond
SESSIOND=sessiond
NS=ns
SESSION_MODULE_LIB=modules/libSessionModules.so

all: session.pb.cc $(SESSION_MODULE_LIB) $(SESSIOND) $(NS)

$(SESSION_MODULE_LIB):
	make -C modules

$(SESSIOND): $(SOURCES) xia.cpp ip.cpp $(SESSION_MODULE_LIB)
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	$(SESSION_MODULE_LIB)

$(NS): $(NS_SOURCES)
	$(CC) -o $@ $(CFLAGS) $(NS_SOURCES) $(LDFLAGS)	

session.pb.cc: session.proto
	protoc --cpp_out=. session.proto

clean:
	-rm $(SESSIOND)
	-rm $(NS)
	-rm -f *.o session.pb.*
	make -C modules clean
