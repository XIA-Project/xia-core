include ../../../xia.mk

.PHONY: all clean

TARGET=dagaddr_example
LDFLAGS += $(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so

all: $(TARGET)

%: %.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(TARGET)
