include ../../../xia.mk

.PHONY: clean

PYTHON=`find /usr/include/python*/Python.h | sed "s/\/Python.h//"`

CFLAGS +=-c -I$(PYTHON) -fpic
LDFLAGS +=-lc -ldl

DAGADDR_LIB=$(XLIB)/libdagaddr.so
LIB=$(XLIB)/_dagaddr.so
STATIC_DAGADDR_LIB=$(XLIB)/libdagaddr.a
STATICLIB=$(XLIB)/_dagaddr.a

OBJS=dagaddr_wrap.o

all: $(LIB) $(XLIB)/dagaddr.py 

static: $(STATICLIB) $(XLIB)/dagaddr.py

%.o: %.cxx
	$(CC) $(CFLAGS) $< -o $@

$(LIB): $(DAGADDR_LIB) $(OBJS)
	$(LD) -shared -o $@ $(OBJS) $(DAGADDR_LIB) -I$(PYTHON) $(LDFLAGS)

$(STATICLIB): $(STATIC_DAGADDR_LIB) $(OBJS)
	ar rcs $@ $(OBJS)

dagaddr_wrap.cxx: dagaddr.i
	swig -c++ -outdir $(XLIB) -python dagaddr.i 

clean: 
	-rm -f $(LIB) $(STATICLIB)
	-rm -f $(XLIB)/dagaddr.py $(XLIB)/dagaddr.pyc
	-rm -f *.so *.o *.pyc
	-rm -f dagaddr_wrap.cxx
