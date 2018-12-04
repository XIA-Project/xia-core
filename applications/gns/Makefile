include ../../xia.mk

.PHONY: clean

CPRDIR=$(XIADIR)/cpr
CPRLIBDIR=$(CPRDIR)/build/lib
CPRINCDIR=$(CPRDIR)/include

CPRLDFLAGS=-L$(CPRLIBDIR) -lcpr -lcurl -Wl,-rpath,$(CPRLIBDIR)

JSONINCDIR=$(XIADIR)/json/single_include/nlohmann

LDFLAGS+=$(LIBS) -lprotobuf -lpthread $(CPRLDFLAGS)
CFLAGS+=-I$(APIDIR)/xcache/ -I$(CPRINCDIR) -I$(JSONINCDIR)

SOURCES=$(wildcard *.cc)
TARGETS=$(SOURCES:.cc=)

all: $(TARGETS)


%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

%: %.cc
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
