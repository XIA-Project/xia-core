include ../../xia.mk
CFLAGS+=-g -I../../../click/include/click
LDFLAGS+=$(LIBS) -ltidy

all: httpd simpleclient

httpd: httpd.c
	g++ $(CFLAGS) -o httpd httpd.c html_parser.c $(LDFLAGS) -g

simpleclient: simpleclient.c
	g++ -W -Wall -I../../api/include -o simpleclient simpleclient.c ../../api/lib/libdagaddr.so

clean:
	rm httpd simpleclient
