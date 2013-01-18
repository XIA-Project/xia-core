.PHONY: clean doc test install
APIDIR=..
XLIB=$(APIDIR)/lib
XINC=$(APIDIR)/include
CC=g++
LD=g++
CFLAGS=-c -Wall -Wextra -Iminini -I$(XINC)
LDFLAGS =-lprotobuf
SOURCES=Xaccept.c Xbind.c Xclose.c Xconnect.c  XrequestChunk.c  \
		XgetChunkStatus.c  XreadChunk.c  XputChunk.c Xrecv.c \
        Xrecvfrom.c Xsend.c Xsendto.c Xsocket.c  Xgetaddrinfo.c \
		Xsetsockopt.c Xutil.c state.c Xinit.c XupdateAD.c XupdateNameServerDAG.c XgetDAGbyName.c  \
		minini/minIni.c 
SHARED_OBJECTS=$(SOURCES:.c=.shared.o) xia.pb.shared.o
STATIC_OBJECTS=$(SOURCES:.c=.o) xia.pb.o
SLIB=$(XLIB)/libXsocket.a
DLIB=$(XLIB)/libXsocket.so

all: xia.pb.cc  $(SLIB) $(DLIB)
	make -C python all

xia.pb.cc: xia.proto
	protoc --cpp_out=.  xia.proto

xia.pb.shared.o:
	$(CC) $(CFLAGS) -fpic xia.pb.cc  -o $@

%.o: %.c X*.h xia.pb.cc xia.pb.h
	$(CC) $(CFLAGS) $(LDFLAGS)  $<  -o $@

%.shared.o: %.c X*.h  xia.pb.cc
	$(CC) $(CFLAGS) -fpic $<  -o $@

$(SLIB): $(STATIC_OBJECTS) 
	ar rcs $@ $(STATIC_OBJECTS) 

$(DLIB): $(SHARED_OBJECTS) 
	$(LD) -shared -o $@ $(SHARED_OBJECTS) $(LDFLAGS) -lc  -l dl

xwrap.so: xwrap.c
	$(CC) -Wall -fPIC -DPIC -c $<
	$(LD) -shared -o $@ xwrap.o -ldl

doc: Xdoc.h $(SOURCES)
	doxygen XsocketsAPI.cfg
	make -C python doc

test: $(DLIB)
	make -C test test

clean:
	-rm -f $(SLIB) $(DLIB)
	-rm -f *.o xia.pb.*
	-rm -rf doc
	make -C python clean
	make -C test clean

