.PHONY: all runtests

APIDIR=../..
XLIB=$(APIDIR)/lib
XINC=$(APIDIR)/include
CC=gcc
CFLAGS=-I.. -I$(XINC) -Wall -Wextra
LIBS =$(XLIB)/libXsocket.so

TARGETS=dag_test addrinfo_test

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

test: $(TARGETS)
	./dag_test
	./addrinfo_test

clean:
	-rm $(TARGETS)
