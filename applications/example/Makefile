.PHONY: clean

CC=g++

APIDIR=../../api
XINC=$(APIDIR)/include
XLIB=$(APIDIR)/lib
BINDIR=../../bin

CFLAGS=-I$(XINC) -Wall -Wextra
LDFLAGS =  -lprotobuf -lpthread $(XLIB)/libXsocket.so
APPS = chunk_copy_client chunk_copy_server echoclient echoclient-datagram echoclient-simple \
		echoserver time_of_day_client time_of_day_server

all: $(APPS)
	mv $(APPS) $(BINDIR)

clean:
	rm -f $(addprefix $(BINDIR)/, $(APPS))


