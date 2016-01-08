include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
SOURCES=xnetjd.cc
NETJD=$(BINDIR)/xnetjd

ndap_pb2.py: ndap.proto
	protoc --python_out=. $^

all: $(NETJD) ndap_pb2.py

$(NETJD): $(SOURCES) $(XINC)/Xsocket.h
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)

clean:
	-rm ndap_pb2.py
