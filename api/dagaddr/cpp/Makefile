include ../../../xia.mk

.PHONY: all clean doc

SOURCES=dagaddr.cpp utils.cpp
OBJECTS=$(SOURCES:.cpp=.o)
LIBS=$(XLIB)/libdagaddr.so
STATICLIB=$(XLIB)/libdagaddr.a

all: $(LIB) dagaddr_test

static: $(STATICLIB) dagaddr_test

%.o: %.cpp utils.hpp $(XINC)/dagaddr.hpp
	$(CC) $(CFLAGS) -c -fpic $<  -o $@

$(LIBS): $(OBJECTS) 
	$(LD) -shared -o $@ $(OBJECTS) -lc -ldl $(LDFLAGS)

$(STATICLIB): $(OBJECTS)
	ar rcs $@ $(OBJECTS)
	make dagaddr_test LIBS='$(STATICLIB)' LIBFLAGS='-Bstatic -lprotobuf'

dagaddr_test: test.cpp $(LIBS)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@ $(LIBS) $(LIBFLAGS)

doc: mainpage.h $(SOURCES)
	doxygen Doxyfile

test: dagaddr_test
	./dagaddr_test

clean:
	-rm -f *.o *.so dagaddr_test $(LIBS) $(STATICLIB)
	-rm -rf ../doc
