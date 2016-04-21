include ../../xia.mk

.PHONY: clean test

CFLAGS=-c -Wall -Wextra -fPIC -DPIC -g -O0 -I$(XINC) -I../xsocket -I../xsocket/minini
LDFLAGS+=-lc -ldl $(XLIB)/libXsocket.so
LIB=$(XLIB)/xwrap.so

ifdef DEBUG
	CFLAGS+=-ggdb -DDEBUG
endif

all: $(LIB)

$(LIB): xwrap.c
	$(CC) $(CFLAGS) $<
	$(LD) -shared -o $@ xwrap.o $(LDFLAGS) 

test: $(LIB)
	make -C test test

clean:
	-rm -f $(LIB)
	-rm -f *.o
	make -C test clean

