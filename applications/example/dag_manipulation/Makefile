include ../../../xia.mk

.PHONY: all clean

TARGET=dagaddr_example
LDFLAGS += $(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so

all: $(TARGET)

%: %.cpp
	$(CC) $(CFLAGS) -std=c++0x -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(TARGET)
