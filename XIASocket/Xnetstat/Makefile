.PHONY: clean

CFLAGS=-I../API
LDFLAGS =  -lprotobuf

all: xnetstat

xnetstat: Xnetstat.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< ../API/libXsocket.a

clean:
	rm -f xnetstat
