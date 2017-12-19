include ../../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS) -lprotobuf -lpthread -lxml2 -lpq
TARGETS = video_publisher manifest_server proxy broker_server
CFLAGS += -I/usr/include/libxml2
CFLAGS += -I/usr/local/include/libxml2
CFLAGS += -I/usr/include/postgresql
CFLAGS += -Wno-write-strings -Wno-sign-compare -std=c++11

all: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

proxy: proxy.o csapp.o utils.o broker_client.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

video_publisher: video_publisher.o manifest.o utils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

manifest_server: manifest_server.o utils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

broker_server: broker_server.o utils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	-rm -f *.o
	-rm -f *.mpd
	-rm -f *.txt
	-rm -f video_publisher
	-rm -f manifest_server
	-rm -f broker_server
	-rm -f proxy
