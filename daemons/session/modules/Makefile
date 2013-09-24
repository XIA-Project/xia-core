include ../../../xia.mk

.PHONY: clean

CFLAGS +=-c -fpic
CPPFLAGS=$(CFLAGS)
LDFLAGS += -lc -ldl -lz -lssl -lcrypto # -lssl and -lcrypto only needed for IP

MODULE_SOURCES=TransportModuleXIA.cpp TransportModuleIP.cpp SSLModule.cpp SSLModuleIP.cpp InterfaceModule.cpp CompressionModule.cpp # SSLModuleXIA.cpp
STACK_INFO_SOURCES=UserLayerInfo.cpp AppLayerInfo.cpp TransportLayerInfo.cpp NetworkLayerInfo.cpp LinkLayerInfo.cpp PhysicalLayerInfo.cpp
SOURCES=$(MODULE_SOURCES) $(STACK_INFO_SOURCES)

MODULE_HEADERS=SessionModule.h TransportModule.h SSLModule.h InterfaceModule.h CompressionModule.h
STACK_INFO_HEADERS=UserLayerInfo.h AppLayerInfo.h TransportLayerInfo.h NetworkLayerInfo.h LinkLayerInfo.h PhysicalLayerInfo.h
HEADERS=$(MODULE_HEADERS) $(STACK_INFO_HEADERS)

OBJS=$(SOURCES:.cpp=.o)
LIB=libSessionModules.so
STATICLIB=libSessionModules.a

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
