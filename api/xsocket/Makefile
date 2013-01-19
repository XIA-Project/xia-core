.PHONY: clean doc test
APIDIR=..
XLIB=$(APIDIR)/lib
XINC=$(APIDIR)/include
CC=g++
LD=g++
CFLAGS=-c -Wall -Wextra -Iminini -I$(XINC)
CPPFLAGS=-c -Wall -Wextra -Iminini -I$(XINC)
LDFLAGS =-lprotobuf
SOURCES=Xaccept.c Xbind.c Xclose.c Xconnect.c  XrequestChunk.c  \
		XgetChunkStatus.c  XreadChunk.c  XputChunk.c Xrecv.c \
        Xrecvfrom.c Xsend.c Xsendto.c Xsocket.c  Xgetaddrinfo.c \
		Xsetsockopt.c Xutil.c state.c Xinit.c XupdateAD.c XupdateNameServerDAG.c XgetDAGbyName.c  \
		minini/minIni.c 
OBJS=$(SOURCES:.c=.o) xia.pb.o
LIB=$(XLIB)/libXsocket.so

all: xia.pb.cc $(LIB)
	make -C python all

xia.pb.cc: xia.proto
	protoc --cpp_out=. xia.proto

xia.pb.o:
	$(CC) $(CFLAGS) -fpic xia.pb.cc  -o $@

%.o: %.c xia.pb.cc
	$(CC) $(CFLAGS) $(LDFLAGS) -fpic $<  -o $@

$(LIB): $(OBJS) 
	$(LD) -shared -o $@ $(OBJS) $(LDFLAGS) -lc  -l dl

xwrap.so: xwrap.c
	$(CC) -Wall -fPIC -DPIC -c $<
	$(LD) -shared -o $@ xwrap.o -ldl

doc: Xdoc.h $(SOURCES)
	doxygen XsocketsAPI.cfg
	make -C python doc

test: $(LIB)
	make -C test test

clean:
	-rm -f $(LIB)
	-rm -f *.o xia.pb.*
	-rm -rf doc
	make -C python clean
	make -C test clean

