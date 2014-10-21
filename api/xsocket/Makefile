include ../../xia.mk

.PHONY: clean doc test

CFLAGS +=-c -Iminini -fpic
CPPFLAGS=$(CFLAGS)
LDFLAGS +=-lprotobuf -lc -ldl -lcrypto -lssl $(XLIB)/libdagaddr.so

SOURCES=Xaccept.c Xbind.c XbindPush.c Xclose.c Xconnect.c  XrequestChunk.c  Xgetaddrinfo.c \
	Xfcntl.c XgetChunkStatus.c  XreadChunk.c  XputChunk.c Xrecv.c \
	Xrecvfrom.c Xsend.c Xsendto.c Xsocket.c  XpushChunkto.c XrecvChunkfrom.c\
	Xsetsockopt.c Xutil.c state.c Xinit.c XupdateAD.c XupdateNameServerDAG.c XgetDAGbyName.c  \
	Xkeys.c Xsecurity.c minini/minIni.c
OBJS=$(SOURCES:.c=.o) xia.pb.o
LIB=$(XLIB)/libXsocket.so
STATICLIB=$(XLIB)/libXsocket.a

all: xia.pb.cc $(LIB)
	make -C python all

static: xia.pb.cc $(STATICLIB)

xia.pb.cc: xia.proto
	protoc --cpp_out=. xia.proto

xia.pb.o:
	$(CC) $(CFLAGS) xia.pb.cc  -o $@

%.o: %.c xia.pb.cc $(XINC)/Xsocket.h $(XINC)/xia.h
	$(CC) $(CFLAGS) $<  -o $@ -lpthread

$(LIB): $(OBJS) 
	$(LD) -shared -o $@ $(OBJS) $(LDFLAGS)

$(STATICLIB): $(OBJS) 
	ar rcs $@ $(OBJS)

doc: Xdoc.h $(SOURCES)
	doxygen XsocketsAPI.cfg
	make -C python doc

unittest: unittest.cpp  $(LIB) $(XINC)/Xsocket.h $(XINC)/xia.h
	$(CC) -I$(XINC) -Wall -Wextra $< -o $@ -lgtest $(LIB) $(XLIB)/libdagaddr.so

test: unittest $(LIB)
	./unittest

clean:
	-rm -f $(LIB)
	-rm -f $(STATICLIB)
	-rm -f *.o xia.pb.*
	-rm -rf doc
	-rm -f unittest
	make -C python clean

