default: XgetDAGbyname

test: test.c XgetDAGbyname 
	gcc -ggdb -I../../XIASocket/API test.c -o tester -Wall -Werror XgetDAGbyname.o ../../XIASocket/API/libXsocket.a -lprotobuf -lpthread

XgetDAGbyname: XgetDAGbyname.c
	gcc -ggdb -I../../XIASocket/API -c XgetDAGbyname.c -Wall -Werror

python-wrapper: XgetDAGbyname.c
	gcc -shared -I../../XIASocket/API XgetDAGbyname.c -o ./python_wrapper/XgetDAGbyname.so -Wall -Werror -fPIC ../../XIASocket/API/libXsocket.a -lprotobuf -lpthread

clean:
	rm tester
	rm *.o
