include ../../../xia.mk

.PHONY: all clean

TARGET=dagaddr_example
LDFLAGS += $(LIBS)
ifeq ($(wildcard $(XLIB)/libdagaddr.a),)
	LDFLAGS += $(XLIB)/libdagaddr.so
else
	LDFLAGS += $(XLIB)/libdagaddr.a -Bstatic
endif

all: $(TARGET)

%: %.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(TARGET)
