.PHONY: clean doc install

XLIB=../../lib
XSOCK_LIB=$(XLIB)/libXsocket.so
TARGET=$(XLIB)/_c_xsocket.so

all: $(TARGET) $(XLIB)/xsocket.py 

$(TARGET): $(XSOCK_LIB) c_xsocket_wrap.c 
	gcc -I/usr/include/python2.7 -fpic -c c_xsocket_wrap.c 
	gcc -shared c_xsocket_wrap.o -lXsocket -o $@  -I/usr/include/python2.7 -L$(XLIB)

c_xsocket_wrap.c: c_xsocket.i $(XSOCK_LIB)
	swig -outdir $(XLIB) -python c_xsocket.i 

$(XLIB)/xsocket.py: oo_wrapper/xsocket.py
	cp $< $@

doc:
	make -C doc html

clean: 
	-rm $(TARGET)
	-rm $(XLIB)/xsocket.py
	-rm $(XLIB)/c_xsocket.py
	make -C doc clean
	-rm -f *.so *.o *.pyc
	-rm c_xsocket_wrap.c
