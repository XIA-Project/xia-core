include ../../xia.mk
CC=g++
.PHONY: all clean

XCACHE=$(BINDIR)/xcache
XCACHEDIR=$(XIADIR)/daemons/xcache
DAEMONS_COMMON_PATH=$(XCACHEDIR)/../common
DAEMONS_COMMON_INCLUDE=$(DAEMONS_COMMON_PATH)
CLICKDIR=$(XIADIR)/click/
CLICKINCLUDE=$(CLICKDIR)/include

CFLAGS+=-g -I$(CLICKINCLUDE) -I$(XCACHEDIR)/common -I$(DAEMONS_COMMON_INCLUDE) -I$(XCACHEDIR)/include -I$(XCACHEDIR)/bin -I$(XCACHEDIR) -I$(XCACHEDIR)/proto
LDFLAGS= -lXsocket -lpthread -lssl -lcrypto -ldagaddr
XCACHE_BINDIR=$(XCACHEDIR)/bin/
DAEMONS_COMMON=$(DAEMONS_COMMON_PATH)/csclient.o $(DAEMONS_COMMON_PATH)/XIARouter.o

export ## Make variables available to subsequent makefiles

TARGETS=proto src api common

.PHONY: $(TARGETS) bin

all: bin
	cp $(XCACHE_BINDIR)/xcache $(XCACHE)

%.o: %.cc
	$(CC) $(CFLAGS) -o $@ -c $^

daemons_common: $(DAEMONS_COMMON)
	mv $(DAEMONS_COMMON_PATH)/*.o $(XCACHE_BINDIR)

$(TARGETS):
	make -C $@
	for f in  $@/*.o; do \
		if [ -e $$f ]; then \
			mv -f $@/*.o $(XCACHE_BINDIR); \
			break; \
		fi; \
	done

bin: daemons_common $(TARGETS)
	make -C $@

clean:
	for dir in ${TARGETS}; do \
		make -C $$dir clean;	\
	done
	make -C bin clean
