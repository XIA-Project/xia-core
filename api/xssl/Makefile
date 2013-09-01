include ../../xia.mk

.PHONY: clean

CFLAGS +=-c
LDFLAGS += -lssl -lcrypto
SOURCES=XSSL_CTX_new.c XSSL_util.c XSSL_new.c XSSL_set_fd.c XSSL_accept.c \
		XSSL_connect.c
OBJS=$(SOURCES:.c=.o)
LIB=$(XLIB)/libXssl.so
STATICLIB=$(XLIB)/libXssl.a

all: $(LIB)

static: $(STATICLIB)

%.o: %.c $(XINC)/xssl.h
	$(CC) $(CFLAGS) $<  -o $@ $(LDFLAGS) -lpthread -lssl -lcrypto

$(LIB): $(OBJS) 
	$(LD) -shared -o $@ $(OBJS) $(XLIB)/libXsocket.so $(LDFLAGS)

$(STATICLIB): $(OBJS) 
	ar rcs $@ $(OBJS)

clean:
	-rm -f $(LIB)
	-rm -f $(STATICLIB)
	-rm -f *.o
