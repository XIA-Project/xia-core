include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xia_routing_handler.cpp
#the associated target file and special libraries
XIAROUTINGHANDLER =$(BINDIR)/xiaroutinghandler 
LDFLAGS += $(LIBS) -lprotobuf -L$(XLIB) -lxcachehdr -lxcache -lXsocket -ldagaddr -licid

# specify the target to capture all as default if you type 'make' wo arguments
all: $(XIAROUTINGHANDLER)

# specific how to compile the target
$(XIAROUTINGHANDLER): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) -Wno-unused-parameter $^ $(LDFLAGS)

clean:
	@-rm -f $(XIROUTINGHANDLER)

