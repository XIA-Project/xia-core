include ../../xia.mk

.PHONY: clean doc test

CFLAGS +=-c -Iminini -fpic
CPPFLAGS=$(CFLAGS)
LDFLAGS +=-lprotobuf -lc -ldl -lcrypto -lssl $(XLIB)/libdagaddr.so

SOURCES= Xaccept.c Xbind.c Xclose.c Xconnect.c Xfcntl.c Xgetaddrinfo.c \
	XgetDAGbyName.c Xinit.c  \
	XdefaultInterface.c \
	Xrecv.c Xselect.c Xsend.c Xsetsockopt.c Xsocket.c \
	XupdateDAG.c XupdateNameServerDAG.c Xutil.c Xlisten.c state.c \
	Xmsg.c Xfork.c Xnotify.c \
	minini/minIni.c XsetXcacheSID.c Xkeys.c Xsecurity.c \
	Xgetifaddrs.c Xgethostname.c

OBJS=$(SOURCES:.c=.o) xia.pb.o
LIB=$(XLIB)/libXsocket.so
STATICLIB=$(XLIB)/libXsocket.a

all: xia.pb.cc $(LIB)
	make -C python all

static: xia.pb.cc $(STATICLIB)

xia.pb.cc: xia.proto
	protoc --cpp_out=. xia.proto

xia.pb.o:
	$(CC) $(CFLAGS) -Wno-unused-parameter xia.pb.cc  -o $@

%.o: %.c xia.pb.cc $(XINC)/Xsocket.h $(XINC)/xia.h
	$(CC) $(CFLAGS) $<  -o $@ -lpthread

$(LIB): $(OBJS)
	$(LD) -shared -o $@ $(OBJS) $(LDFLAGS)

$(STATICLIB): $(OBJS)
	ar rcs $@ $(OBJS)

doc:
	make -C python doc

unittest: unittest.cpp  $(LIB) $(XINC)/Xsocket.h $(XINC)/xia.h
	$(CC) -I$(XINC) -I../gtest/include -Wall -Wextra $< -o $@ ../gtest/libgtest.a $(LIB) $(XLIB)/libdagaddr.so

test: unittest $(LIB)
	./unittest

clean:
	-rm -f $(LIB)
	-rm -f $(STATICLIB)
	-rm -f *.o xia.pb.*
	-rm -f unittest
	make -C python clean
