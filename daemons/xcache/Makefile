include ../../xia.mk
CC=g++
.PHONY: all clean

XCACHEDIR=$(XIADIR)/daemons/xcache
COMMON_PATH=$(XCACHEDIR)/../common
COMMON_INCLUDE=$(COMMON_PATH)
CLICKDIR=$(XIADIR)/click-2.0.1/
CLICKINCLUDE=$(CLICKDIR)/include

CFLAGS+=-g -I$(CLICKINCLUDE) -I$(XCACHEDIR)/common -I$(COMMON_INCLUDE) -I$(XCACHEDIR)/include -I$(XCACHEDIR)/bin -I$(XCACHEDIR) -I$(XCACHEDIR)/proto -Wall -Werror
LDFLAGS= $(LIBS) -lpthread
XCACHE_BINDIR=$(XCACHEDIR)/bin/
COMMON=$(COMMON_PATH)/csclient.o $(COMMON_PATH)/XIARouter.o

export ## Make variables available to subsequent makefiles


TARGETS=proto src api common

.PHONY: $(TARGETS) bin

all: bin


%.o: %.cc
	$(CC) $(CFLAGS) -o $@ -c $^

common: $(COMMON)
	mv $(COMMON_PATH)/*.o $(XCACHE_BINDIR) 

$(TARGETS):
	make -C $@
	for f in  $@/*.o; do \
		if [ -e $$f ]; then \
			mv -f $@/*.o $(XCACHE_BINDIR); \
			break; \
		fi; \
	done

bin: common $(TARGETS)
	make -C $@

clean:
	for dir in ${TARGETS}; do \
		make -C $$dir clean;	\
	done
	make -C bin clean
