include ../../../xia.mk

.PHONY: clean doc

XSOCK_LIB=$(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so
TARGET=$(XLIB)/_c_xsocket.so
LDFLAGS +=-lc -ldl $(PYLDFLAGS)
PYTHON=$(PYINCLUDES)
CFLAGS += -Wno-unused-parameter

ifeq ($(wildcard $(XLIB)/libXsocket.a),)
	PY_LIBS=$(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so
else
	PY_LIBS=$(XLIB)/libXsocket.a $(XLIB)/libdagaddr.a -Bstatic -lprotobuf
endif

all: $(TARGET) $(XLIB)/xsocket.py

$(TARGET): $(XSOCK_LIB) c_xsocket_wrap.c
	$(CC) $(CFLAGS) -I $(XINC) $(PYTHON) -fpic -c c_xsocket_wrap.c
	$(CC) -shared c_xsocket_wrap.o $(PY_LIBS) -o $@  $(PYTHON) $(LDFLAGS)

c_xsocket_wrap.c: c_xsocket.i $(XSOCK_LIB)
	swig -outdir $(XLIB) -python c_xsocket.i

$(XLIB)/xsocket.py: oo_wrapper/xsocket.py
	cp $< $@

doc:
	make -C doc html

clean:
	-rm -f $(TARGET)
	-rm -f $(XLIB)/xsocket.py
	-rm -f $(XLIB)/c_xsocket.py
	-rm -f $(XLIB)/*.pyc
	make -C doc clean
	-rm -f *.so *.o *.pyc
	-rm -f c_xsocket_wrap.c
