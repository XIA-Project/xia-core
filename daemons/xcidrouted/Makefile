all:
	mkdir -p bin tmp stats workload conf log
	make -C src

static: all

clean:
	make -C src clean
	rm -f ./bin/XIACIDWGen
	rm -f ./bin/client
	rm -f ./bin/server
	rm -f ./bin/xcidrouted