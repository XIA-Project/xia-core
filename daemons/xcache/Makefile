include ../../xia.mk
.PHONY: all clean

XCACHE=$(BINDIR)/xcache
XCACHEDIR=$(XIADIR)/daemons/xcache
DAEMONS_COMMON_PATH=$(XCACHEDIR)/../common
DAEMONS_COMMON_INCLUDE=$(DAEMONS_COMMON_PATH)
CLICKDIR=$(XIADIR)/click/
CLICKINCLUDE=$(CLICKDIR)/include

CFLAGS+=-Wall -Wextra -I$(XINC) -I$(XCACHEINC) -L$(XLIB) -L$(XCACHEAPIDIR)
CFLAGS+=-g -I$(CLICKINCLUDE) -I$(XCACHEDIR)/common -I$(DAEMONS_COMMON_INCLUDE) -I$(XCACHEDIR)/include -I$(XCACHEDIR)/bin -I$(XCACHEDIR) -I$(XCACHEDIR)/proto
LDFLAGS+= -lXsocket -lpthread -lssl -lcrypto -ldagaddr
XCACHE_BINDIR=$(XCACHEDIR)/bin/
DAEMONS_COMMON=$(DAEMONS_COMMON_PATH)/csclient.o $(DAEMONS_COMMON_PATH)/XIARouter.o

export ## Make variables available to subsequent makefiles

TARGETS=proto src api bin

.PHONY: all $(TARGETS)

XCACHE=xcache

$(XCACHE): $(TARGETS)
	cp $(XCACHE_BINDIR)/xcache $(XCACHE)

all: $(XCACHE)

%.o: %.cc
	$(CC) $(CFLAGS) -o $@ -c $^

$(TARGETS):
	make -C $@

clean:
	for dir in ${TARGETS}; do \
		make -C $$dir clean;	\
	done
