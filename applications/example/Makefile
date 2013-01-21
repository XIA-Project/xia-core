.PHONY: clean

CC=g++

XIADIR=$(shell cd ../../ && pwd)

APIDIR=$(XIADIR)/api
XINC=$(APIDIR)/include
XLIB=$(APIDIR)/lib
BINDIR=.
CFLAGS=-Wall -Wextra -I$(XINC)
LDFLAGS=-lprotobuf -lpthread
LIBS=$(XLIB)/libXsocket.so

TARGETS= chunk_copy_client chunk_copy_server echoclient echoclient-datagram \
	echoclient-simple echoserver time_of_day_client time_of_day_server

all: $(addprefix $(BINDIR)/, $(TARGETS))
	make -C dag_manipulation

$(BINDIR)/%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LIBS)

$(BINDIR)/%: %.cpp
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LIBS)

clean: 
	-rm $(addprefix $(BINDIR)/, $(TARGETS))
	make -C dag_manipulation clean
