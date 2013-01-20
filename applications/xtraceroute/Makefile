XIADIR=$(shell cd ../../ && pwd)

APIDIR=$(XIADIR)/api
XLIB=$(APIDIR)/lib
XINC=$(APIDIR)/include
BINDIR=$(XIADIR)/bin
CFLAGS=-Wall -Wextra -I$(XINC)
LDFLAGS=-lpthread
LIBS=$(XLIB)/libXsocket.so $(XLIB)/libXIAResolver.so

OUT=xtraceroute
TARGETS=$(addprefix $(BINDIR)/, $(OUT))
SOURCES=$(addsuffix .c, $(OUT))

all: $(TARGETS)

$(TARGETS): $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

clean: 
	-rm $(TARGETS)