.PHONY: clean
CC=g++
CFLAGS=-c -Wall -I../minini
SOURCES=Xaccept.c Xbind.c Xclose.c Xconnect.c  XgetCID.c XputCID.c Xrecv.c\
	Xrecvfrom.c Xsend.c Xsendto.c Xsocket.c ../minini/minIni.c
OBJECTS=$(SOURCES:.c=.o)

all: libXsocket.a

%.o: %.c X*.h 
	$(CC) $(CFLAGS)  $<  -o $@

libXsocket.a: $(OBJECTS)
	ar rcs $@ $(OBJECTS) 

clean:
	rm -f *.o *.a

pc: 
	gcc -o pc pingClient.c libXsocket.a
