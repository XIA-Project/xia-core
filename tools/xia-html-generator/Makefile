include ../../xia.mk

LDFLAGS+=$(LIBS)
LDFLAGS+=-ltidy -lssl -lcrypto
CFLAGS+=-g
OBJS=xia_html_gen.o

xia_html_gen: $(OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

*.o: *.c
	$(CC) -o $^ %@ $(CFLAGS)

clean:
	rm -f $(OBJS)
	rm -f xia_html_gen
