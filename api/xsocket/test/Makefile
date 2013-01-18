.PHONY: all runtests

CC=gcc
CFLAGS=-I.. -Wall -Wextra
LIBS =../libXsocket.so

TARGETS=dag_test addrinfo_test

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

test: $(TARGETS)
	./dag_test
	./addrinfo_test

clean:
	rm $(TARGETS)
