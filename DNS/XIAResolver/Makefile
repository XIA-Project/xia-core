default: XgetDAGbyname

test: test.c XgetDAGbyname 
	gcc -ggdb test.c -o tester -Wall -Werror XgetDAGbyname.o

XgetDAGbyname: XgetDAGbyname.c
	gcc -ggdb -c XgetDAGbyname.c -Wall -Werror

python-wrapper: XgetDAGbyname.c
	gcc -shared XgetDAGbyname.c -o ./python_wrapper/XgetDAGbyname.so -Wall -Werror -fPIC

clean:
	rm tester
	rm *.o
