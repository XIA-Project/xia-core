include ../../xia.mk

CC=g++
LDFLAGS += $(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so

OUT=xtraceroute
TARGETS=$(addprefix $(BINDIR)/, $(OUT))
SOURCES=$(addsuffix .c, $(OUT))

all: $(TARGETS)

$(TARGETS): $(SOURCES)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) 

clean: 
	-rm $(TARGETS)
