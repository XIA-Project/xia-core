include ../../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)
TARGETS = video_publisher manifest_server proxy

all: $(TARGETS)

csapp.o: csapp.c csapp.h
		$(CC) $(CFLAGS) -Wno-write-strings -Wno-sign-compare -c csapp.c

manifest.o: manifest.c manifest.h
		$(CC) $(CFLAGS) -I/usr/local/include/libxml2 -Wno-write-strings -Wno-sign-compare -c manifest.c

utils.o: utils.c utils.h
		$(CC) $(CFLAGS) -Wno-write-strings -Wno-sign-compare -c utils.c

video_publisher.o: video_publisher.c video_publisher.h utils.h manifest.h
		$(CC) $(CFLAGS) -c video_publisher.c -I/usr/local/include/libxml2 -Wno-unused-parameter

manifest_server.o: manifest_server.c manifest_server.h utils.h 
		$(CC) $(CFLAGS) -c manifest_server.c

proxy.o: proxy.c proxy.h csapp.h utils.h
		$(CC) $(CFLAGS) -Wno-write-strings -Wno-sign-compare -c proxy.c

proxy: proxy.o csapp.o utils.o $(LDFLAGS) -lprotobuf -lpthread

video_publisher: video_publisher.o manifest.o utils.o $(LDFLAGS) -lxml2 -lprotobuf -lpthread

manifest_server: manifest_server.o utils.o $(LDFLAGS) -lprotobuf -lpthread

clean:
	-rm -f *.o
	-rm -f *.mpd
	-rm -f *.txt
	-rm -f video_publisher
	-rm -f manifest_server
	-rm -f proxy