include ../../xia.mk

.PHONY: clean

CPRDIR=$(XIADIR)/cpr
CPRLIBDIR=$(CPRDIR)/build/lib
CPRINCDIR=$(CPRDIR)/include

CPRLDFLAGS=-L$(CPRLIBDIR) -lcpr -lcurl -Wl,-rpath,$(CPRLIBDIR)

LDFLAGS+=$(LIBS) -lprotobuf -lpthread $(CPRLDFLAGS)
CFLAGS+=-I$(APIDIR)/xcache/ -I$(CPRINCDIR)

TARGETS=gns-demo-server gns-demo-client

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

%: %.cc
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
