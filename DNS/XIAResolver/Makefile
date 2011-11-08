default: getXIPinfo 

tester: getXIPinfo_test.c getXIPinfo
	gcc -ggdb getXIPinfo_test.c -o tester -Wall -Werror getXIPinfo.o

getXIPinfo: getXIPinfo.c
	gcc -ggdb -c getXIPinfo.c -Wall -Werror

python-wrapper: getXIPinfo.c
	gcc -shared getXIPinfo.c -o ./python_wrapper/getXIPinfo.so -Wall -Werror -fPIC

clean:
	rm tester
	rm *.o
