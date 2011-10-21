default: getXIPinfo 

tester: getXIPinfo_test.c getXIPinfo
	gcc -ggdb getXIPinfo_test.c -o tester -Wall -Werror getXIPinfo.o

getXIPinfo: getXIPinfo.c
	gcc -ggdb -c getXIPinfo.c -Wall -Werror

clean:
	rm tester
	rm *.o
