_xsocket.so:
	swig -python xsocket.i 
	gcc -I/usr/include/python2.7 -fpic -c xsocket_wrap.c 
	gcc -shared xsocket_wrap.o ../libXsocket.so -o _xsocket.so  -I/usr/include/python2.7

clean: 
	rm -f *.so *.o *.pyc
