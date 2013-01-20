.PHONY: clean

CC=g++

XIADIR=$(shell cd ../../ && pwd)

APIDIR=$(XIADIR)/api
XINC=$(APIDIR)/include
XLIB=$(APIDIR)/lib
BINDIR=$(XIADIR)/bin
CFLAGS=-Wall -Wextra -I$(XINC)
LDFLAGS=-lprotobuf -lpthread
LIBS=$(XLIB)/libXsocket.so $(XLIB)/libXIAResolver.so

OUT= chunk_copy_client chunk_copy_server echoclient echoclient-datagram \
	echoclient-simple echoserver time_of_day_client time_of_day_server
TARGETS=$(addprefix $(BINDIR)/, $(OUT))
SOURCES=$(addsuffix .c, $(OUT))

all: $(TARGETS)
	make -C dag_manipulation

$(TARGETS): $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

clean: 
	-rm $(TARGETS)
	make -C dag_manipulation clean