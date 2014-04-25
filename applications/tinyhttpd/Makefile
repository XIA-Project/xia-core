all: httpd simpleclient simpleclient_orig

httpd: httpd.c
	g++ -W -Wall -I../../api/include -o httpd httpd.c -lpthread ../../api/lib/libdagaddr.so

simpleclient: simpleclient.c
	g++ -W -Wall -I../../api/include -o simpleclient simpleclient.c ../../api/lib/libdagaddr.so

simpleclient_orig: simpleclient_orig.c
	g++ -W -Wall -o simpleclient_orig simpleclient_orig.c
clean:
	rm httpd simpleclient simpleclient_orig
