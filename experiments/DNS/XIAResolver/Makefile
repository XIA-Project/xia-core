tester: test.c XgetDAGbyname 
	gcc -ggdb -I../../XIASocket/API test.c -o tester -Wall -Werror XgetDAGbyname.o ../../XIASocket/API/libXsocket.a -lprotobuf -lpthread

XgetDAGbyname: XgetDAGbyname.c
	gcc -ggdb -I../../XIASocket/API -c XgetDAGbyname.c -Wall -Werror

clean:
	rm tester
	rm *.o
