INCLUDES=../include
CFLAGS=-c -Wall -Wextra -fpic -I$(INCLUDES)

all: libXIAResolver.so

libXIAResolver.so: XgetDAGbyname.o
	$(LD) -shared -o $@ XgetDAGbyname.o -lc -ldl
	mv $@ ../lib

clean:
	rm -f *.o *.so
	rm ../lib/libXIAResolver.so