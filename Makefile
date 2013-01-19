.PHONY: all clean test

all:
	cd click && ./conf_user_click.sh && make -j8
	make -C api
	make -C applications
	make -C daemons

clean:
	make -C click clean
	make -C api clean
	make -C applications clean
	make -C daemons clean

test:
	make -C api/xsocket test
