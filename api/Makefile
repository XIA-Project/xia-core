all: 
	make -C dagaddr
	make -C xsocket
	make -C wrapper
	make -C xssl

static:
	make -C dagaddr static
	make -C xsocket static
	make -C xssl static

clean:
	make -C dagaddr clean
	make -C xsocket clean
	make -C wrapper clean
	make -C xssl clean

test:
	make -C dagaddr test
	make -C xsocket test
	make -C wrapper test

