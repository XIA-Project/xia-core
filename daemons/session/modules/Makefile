#include ../../xia.mk

.PHONY: clean

CFLAGS +=-c -fpic
CPPFLAGS=$(CFLAGS)
LDFLAGS += -lc -ldl

SOURCES=ServicePerformanceModule.cpp SecurityPrivacyModule.cpp
HEADERS=ContextModule.h SecurityPrivacyModule.h
OBJS=$(SOURCES:.cpp=.o)
LIB=libContextModules.so
STATICLIB=libContextModules.a

all: $(LIB)

static: $(STATICLIB)

%.o: %.cpp $(HEADERS)
	$(CC) $(CPPFLAGS) $<  -o $@

$(LIB): $(OBJS) 
	$(LD) -shared -o $@ $(OBJS) $(LDFLAGS)

$(STATICLIB): $(OBJS) 
	ar rcs $@ $(OBJS)

clean:
	-rm -f $(LIB)
	-rm -f $(STATICLIB)
	-rm -f *.o 
