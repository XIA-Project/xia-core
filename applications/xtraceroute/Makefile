include ../../xia.mk

CC=gcc
LDFLAGS += $(XLIB)/libXsocket.so $(XLIB)/libXIAResolver.so

OUT=xtraceroute
TARGETS=$(addprefix $(BINDIR)/, $(OUT))
SOURCES=$(addsuffix .c, $(OUT))

all: $(TARGETS)

$(TARGETS): $(SOURCES)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) 

clean: 
	-rm $(TARGETS)
