include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xiaforwardingtable.cpp
#the associated target file and special libraries
XIAFORWARDINGTABLE =$(BINDIR)/xiaforwardingtable 
LDFLAGS += $(LIBS) -lprotobuf -L$(XLIB) -lxcachehdr -lxcache -lXsocket -ldagaddr -licid

# specify the target to capture all as default if you type 'make' wo arguments
all: $(XIAFORWARDINGTABLE)

# specific how to compile the target
$(XIAFORWARDINGTABLE): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) -Wno-unused-parameter $^ $(LDFLAGS)

clean:
	@-rm -f $(XIAFORWARDINGTABLE)

