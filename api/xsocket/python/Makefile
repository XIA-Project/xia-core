include ../../../xia.mk

.PHONY: clean doc

XSOCK_LIB=$(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so
TARGET=$(XLIB)/_c_xsocket.so
CC=g++
PYTHON=`find /usr/include/python*/Python.h | sed "s/\/Python.h//"`

all: $(TARGET) $(XLIB)/xsocket.py 

$(TARGET): $(XSOCK_LIB) c_xsocket_wrap.c 
	$(CC) -I $(XINC) -I $(PYTHON) -fpic -c c_xsocket_wrap.c 
	$(CC) -shared c_xsocket_wrap.o $(LIBS) -o $@  -I $(PYTHON)

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
	-rm $(XLIB)/*.pyc
	make -C doc clean
	-rm -f *.so *.o *.pyc
	-rm c_xsocket_wrap.c
