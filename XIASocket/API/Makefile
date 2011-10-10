.PHONY: clean
CC=g++
LD=ld
CFLAGS=-c -Wall -I../minini
SOURCES=Xaccept.c Xbind.c Xclose.c Xconnect.c  XgetCID.c XputCID.c Xrecv.c\
	Xrecvfrom.c Xsend.c Xsendto.c Xsocket.c ../minini/minIni.c

SHARED_OBJECTS=$(SOURCES:.c=.shared.o)
STATIC_OBJECTS=$(SOURCES:.c=.o)

all: libXsocket.a libXsocket.so

%.o: %.c X*.h 
	$(CC) $(CFLAGS)  $<  -o $@

%.shared.o: %.c X*.h 
	$(CC) $(CFLAGS) -fpic $<  -o $@

libXsocket.a: $(STATIC_OBJECTS)
	ar rcs $@ $(STATIC_OBJECTS) 

libXsocket.so: $(SHARED_OBJECTS)
	$(LD) -shared -o $@ $(SHARED_OBJECTS) 

clean:
	rm -f *.o *.a *.so *.shared.o

