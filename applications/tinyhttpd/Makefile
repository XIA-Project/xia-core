include ../../xia.mk

all: httpd simpleclient

httpd: httpd.c
	$(CC) $(CFLAGS) -o httpd httpd.c -lpthread ../../api/lib/libdagaddr.so

simpleclient: simpleclient.c
	$(CC) $(CFLAGS) -o simpleclient simpleclient.c ../../api/lib/libdagaddr.so

clean:
	rm httpd simpleclient
