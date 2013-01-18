.PHONY: clean doc

XSOCK_LIB=../libXsocket.so

all: _c_xsocket.so

_c_xsocket.so: $(XSOCK_LIB) c_xsocket_wrap.c 
	gcc -I/usr/include/python2.7 -fpic -c c_xsocket_wrap.c 
	gcc -shared c_xsocket_wrap.o -lXsocket -o _c_xsocket.so  -I/usr/include/python2.7 -L../

c_xsocket_wrap.c: c_xsocket.i $(XSOCK_LIB)
	swig -python c_xsocket.i 

doc:
	make -C doc html

clean: 
	make -C doc clean
	rm -f *.so *.o *.pyc
	rm c_xsocket_wrap.c
	rm c_xsocket.py
