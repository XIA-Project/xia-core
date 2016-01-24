include ../../../xia.mk

.PHONY: clean doc

XSOCK_LIB=$(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so
TARGET=$(XLIB)/_c_xsocket.so
CC=g++
LDFLAGS +=-lc -ldl `python2-config --ldflags`
PYTHON=`python2-config --includes`

ifeq ($(wildcard $(XLIB)/libXsocket.a),)
	PY_LIBS=$(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so
else
	PY_LIBS=$(XLIB)/libXsocket.a $(XLIB)/libdagaddr.a -Bstatic -lprotobuf
endif

all: $(TARGET) $(XLIB)/xsocket.py

$(TARGET): $(XSOCK_LIB) c_xsocket_wrap.c
	$(CC) -I $(XINC) $(PYTHON) -fpic -c c_xsocket_wrap.c
	$(CC) -shared c_xsocket_wrap.o $(PY_LIBS) -o $@  $(PYTHON) $(LDFLAGS)

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
