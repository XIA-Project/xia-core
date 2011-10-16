default: xiadns

xiadns: getxiadstdagbyname
	gcc xiadns.c -o tester -ggdb -Wall -Werror getxiadstdagbyname.o

getxiadstdagbyname: getxiadstdagbyname.c
	gcc -c -ggdb getxiadstdagbyname.c -Wall -Werror

clean:
	rm tester
	rm *.o
