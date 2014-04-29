include ../../../xia.mk 

.PHONY: all test

PRELOAD=$(XLIB)/xwrap.so $(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so

TARGET=xwtest

all: $(TARGET)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(XLIB)/libdagaddr.so

test: $(TARGET)
	LD_PRELOAD="$(LIBS)"; ./$(TARGET)

clean:
	-rm $(TARGET)
