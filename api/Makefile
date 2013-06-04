all: 
	make -C dagaddr
	make -C xsocket
	make -C wrapper
	make -C session

static:
	make -C dagaddr static
	make -C xsocket static
	make -C session static

clean:
	make -C dagaddr clean
	make -C xsocket clean
	make -C wrapper clean
	make -C session clean

test:
	make -C dagaddr test
	make -C xsocket test
	make -C wrapper test
	make -C session test
