.PHONY: clean
CC=g++
LD=g++
CFLAGS=-c -Wall -Wextra -I../minini
LDFLAGS =  -lprotobuf
SOURCES=Xaccept.c Xbind.c Xclose.c Xconnect.c  XgetCID.c XputCID.c Xrecv.c\
        Xrecvfrom.c Xsend.c Xsendto.c Xsocket.c Xgetsocketidlist.c Xgetsocketinfo.c ../minini/minIni.c

SHARED_OBJECTS=$(SOURCES:.c=.shared.o) xia.pb.shared.o
STATIC_OBJECTS=$(SOURCES:.c=.o) xia.pb.o

all: xia.pb.cc  libXsocket.a libXsocket.so
	make -C swig all

xia.pb.cc: xia.proto
	protoc --cpp_out=.  xia.proto

xia.pb.shared.o:
	$(CC) $(CFLAGS) -fpic xia.pb.cc  -o $@

%.o: %.c X*.h xia.pb.cc xia.pb.h
	$(CC) $(CFLAGS) $(LDFLAGS)  $<  -o $@

%.shared.o: %.c X*.h 
	$(CC) $(CFLAGS) -fpic $<  -o $@

libXsocket.a: $(STATIC_OBJECTS) 
	ar rcs $@ $(STATIC_OBJECTS) 

libXsocket.so: $(SHARED_OBJECTS) 
	$(LD) -shared -o $@ $(SHARED_OBJECTS) $(LDFLAGS) -lc  -l dl

clean:
	rm -f *.o *.a *.so *.shared.o xia.pb.*
	make -C swig clean

