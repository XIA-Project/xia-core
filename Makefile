.PHONY: all clean test

all:
	make -C api/xsocket
	make -C daemons

clean:
	make -C api/xsocket clean
	make -C daemons clean

test:
	make -C api/xsocket test
