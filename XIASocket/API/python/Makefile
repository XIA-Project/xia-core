.PHONY: clean doc

all: _xsocket.so

_xsocket.so: ../libXsocket.so xsocket_wrap.c 
	gcc -I/usr/include/python2.7 -fpic -c xsocket_wrap.c 
	gcc -shared xsocket_wrap.o -lXsocket -o _xsocket.so  -I/usr/include/python2.7 -L../

xsocket_wrap.c: xsocket.i 
	swig -python xsocket.i 

doc:
	make -C doc html

clean: 
	make -C doc clean
	rm -f *.so *.o *.pyc
	rm xsocket_wrap.c
	rm xsocket.py
