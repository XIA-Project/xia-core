include ../xia.mk
CC=g++
XCACHEDIR=$(PWD)
CLICKDIR=$(PWD)/../click-2.0.1/
CFLAGS+=-g -I$(CLICKDIR)/include/click -I$(XCACHEDIR)/include -I$(XCACHEDIR)/bin -I$(XCACHEDIR) -I$(XCACHEDIR)/proto -Wall -Werror -fdiagnostics-color -static
LDFLAGS= $(LIBS)
export ## Make variables available to subsequent makefiles

TARGETS=proto src api

.PHONY: $(TARGETS) bin

all: bin
$(TARGETS):
	make -C $@
	for f in  $@/*.o; do \
		if [ -e $$f ]; then \
			mv -f $@/*.o bin/; \
			break; \
		fi; \
	done

bin: $(TARGETS)
	make -C $@

clean:
	for dir in ${TARGETS}; do \
		make -C $$dir clean;	\
	done
	make -C bin clean
