tester: test.c XregisterName
	gcc -ggdb -I../../XIASocket/API test.c -o tester -Wall -Werror XregisterName.o ../../XIASocket/API/libXsocket.a -lprotobuf -lpthread

XregisterName: XregisterName.c
	gcc -ggdb -I../../XIASocket/API -c XregisterName.c -Wall -Werror

clean:
	rm tester
	rm *.o
