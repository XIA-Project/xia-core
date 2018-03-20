include ../../../xia.mk

VPATH=$(XIADIR)/daemons/broker

.PHONY: clean all

LDFLAGS += $(LIBS) -lprotobuf -lpthread -lxml2
BINARIES = video_publisher manifest_server xiaproxy
TARGETS = $(addprefix $(BINDIR)/, $(BINARIES))
CFLAGS += -I/usr/include/libxml2
CFLAGS += -I/usr/local/include/libxml2
CFLAGS += -Wno-write-strings -Wno-sign-compare -std=c++11
CFLAGS += -I$(APIDIR)/xsocket/minini

all: $(TARGETS) cdn.pb.cc

cdn.pb.cc cdn.pb.h: cdn.proto
	protoc -I$(XIADIR)/daemons/broker --cpp_out=. $<

%.o: %.c cdn.pb.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cc cdn.pb.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/xiaproxy: xiaproxy.o csapp.o utils.o cdn.pb.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BINDIR)/video_publisher: video_publisher.o manifest.o utils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BINDIR)/manifest_server: manifest_server.o utils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	@-rm -f *.o
	@-rm -f *.mpd
	@-rm -f *.txt
	@-rm -f cdn.pb.*
	@-rm -f $(TARGETS)
