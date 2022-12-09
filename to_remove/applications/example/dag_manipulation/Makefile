include ../../../xia.mk

.PHONY: all clean

TARGETS=dag_examples dagaddr_example
LDFLAGS += $(LIBS)
CFLAGS += -Wno-unused-result

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

%: %.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lpthread

clean:
	-rm -f $(TARGET)
