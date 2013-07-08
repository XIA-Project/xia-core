all: httpd simpleclient

httpd: httpd.c
	g++ -W -Wall -I../../api/include -o httpd httpd.c -lpthread ../../api/lib/libdagaddr.so

simpleclient: simpleclient.c
	g++ -W -Wall -I../../api/include -o simpleclient simpleclient.c ../../api/lib/libdagaddr.so

clean:
	rm httpd simpleclient
