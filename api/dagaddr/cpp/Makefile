.PHONY: clean doc
APIDIR=../..
XLIB=$(APIDIR)/lib
XINC=$(APIDIR)/include

CC=g++
LD=g++
CFLAGS=-I$(XINC)
LDFLAGS=-std=c++0x -L$(XLIB)
SOURCES=dagaddr.cpp utils.cpp
OBJECTS=$(SOURCES:.cpp=.o)
TARGET=$(XLIB)/libdagaddr.so

all: $(TARGET) dagaddr_test

%.o: %.cpp utils.hpp $(XINC)/dagaddr.hpp
	$(CC) $(CFLAGS) -c -fpic $<  -o $@ $(LDFLAGS)

$(TARGET): $(OBJECTS) 
	$(LD) -shared -o $@ $(OBJECTS) $(LDFLAGS) -lc -l dl

dagaddr_test: test.cpp
	$(CC) $(CFLAGS) $< -ldagaddr $(LDFLAGS) -o $@

doc: mainpage.h $(SOURCES)
	doxygen Doxyfile

clean:
	-rm -f *.o *.so dagaddr_test $(TARGET)
	-rm -rf ../doc
