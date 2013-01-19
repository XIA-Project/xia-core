all: 
	make -C xsocket
	make -C XIAResolver
	cd dagaddr/cpp && cmake . && cmake . && make && make install

clean:
	make -C xsocket clean
	make -C XIAResolver clean
	make -C dagaddr clean