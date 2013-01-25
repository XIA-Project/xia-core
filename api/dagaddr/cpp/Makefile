include ../../../xia.mk

.PHONY: all clean doc

CFLAGS += -std=c++0x
SOURCES=dagaddr.cpp utils.cpp
OBJECTS=$(SOURCES:.cpp=.o)
TARGET=$(XLIB)/libdagaddr.so

all: $(TARGET) dagaddr_test

%.o: %.cpp utils.hpp $(XINC)/dagaddr.hpp
	$(CC) $(CFLAGS) -c -fpic $<  -o $@

$(TARGET): $(OBJECTS) 
	$(LD) -shared -o $@ $(OBJECTS) -lc -ldl $(LDFLAGS)

dagaddr_test: test.cpp
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@ $(TARGET)

doc: mainpage.h $(SOURCES)
	doxygen Doxyfile

test: dagaddr_test
	./dagaddr_test

clean:
	-rm -f *.o *.so dagaddr_test $(TARGET)
	-rm -rf ../doc
