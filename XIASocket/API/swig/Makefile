_xsocket.so: ../libXsocket.so xsocket.i
	swig -python xsocket.i 
	gcc -I/usr/include/python2.7 -fpic -c xsocket_wrap.c 
	gcc -shared xsocket_wrap.o -lXsocket -o _xsocket.so  -I/usr/include/python2.7 -L../

clean: 
	rm -f *.so *.o *.pyc
