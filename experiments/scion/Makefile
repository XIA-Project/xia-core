
#TARGETS=server client
SOURCES=client server
CC=g++
CFLAGS=-Wall -Wextra -ggdb -I../../api/include
LIBS=../../api/lib/libXsocket.so ../../api/lib/libdagaddr.so
LDFLAGS=

all: $(SOURCES)

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@

clean:
	rm -rf server client
