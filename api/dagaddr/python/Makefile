include ../../../xia.mk

.PHONY: clean

DAGADDR_LIB=$(XLIB)/libdagaddr.so
TARGET=$(XLIB)/_dagaddr.so

all: $(TARGET) $(XLIB)/dagaddr.py 

$(TARGET): $(DAGADDR_LIB) dagaddr_wrap.cxx
	gcc -I/usr/include/python2.7 -fpic -c dagaddr_wrap.cxx -std=c++0x
	gcc -shared dagaddr_wrap.o $(DAGADDR_LIB) -o $@  -I/usr/include/python2.7

dagaddr_wrap.cxx: dagaddr.i $(DAGADDR_LIB)
	swig -c++ -outdir $(XLIB) -python dagaddr.i 

clean: 
	-rm -f $(TARGET)
	-rm -f $(XLIB)/dagaddr.py $(XLIB)/dagaddr.pyc
	-rm -f *.so *.o *.pyc
	-rm -f dagaddr_wrap.cxx
