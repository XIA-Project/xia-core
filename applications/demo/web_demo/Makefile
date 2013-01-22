.PHONY: clean

APIDIR=../../../api
XINC=$(APIDIR)/include
XLIB=$(APIDIR)/lib

CC=g++
CFLAGS=-I$(XINC)
LDFLAGS = -L$(XLIB) -lprotobuf -lpthread -lXsocket
APPS = vs

all: $(APPS)

vs: video_server.c
	g++ $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(APPS)
