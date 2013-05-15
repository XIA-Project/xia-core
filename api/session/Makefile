include ../../xia.mk

.PHONY: clean 

CFLAGS +=-c -Iminini -fpic
CPPFLAGS=$(CFLAGS)
LDFLAGS +=-lprotobuf -lc -ldl

SOURCES=SnewContext.c Sinit.c Sutil.c Sbind.c SacceptConnReq.c api_init.c \
		minini/minIni.c
OBJS=$(SOURCES:.c=.o) session.pb.o
LIB=$(XLIB)/libSession.so
STATICLIB=$(XLIB)/libSession.a

all: session.pb.cc $(LIB)

static: session.pb.cc $(STATICLIB)

session.pb.cc: session.proto
	protoc --cpp_out=. session.proto

session.pb.o:
	$(CC) $(CFLAGS) session.pb.cc  -o $@

%.o: %.c session.pb.cc $(XINC)/session.h
	$(CC) $(CFLAGS) $<  -o $@ -lpthread

$(LIB): $(OBJS) 
	$(LD) -shared -o $@ $(OBJS) $(LDFLAGS)

$(STATICLIB): $(OBJS) 
	ar rcs $@ $(OBJS)

clean:
	-rm -f $(LIB)
	-rm -f $(STATICLIB)
	-rm -f *.o session.pb.*
