all: nameserver xhcp xrouted

clean: nameserver_clean xhcp_clean xrouted_clean

.PHONY: nameserver
nameserver:
	make -C $@;

.PHONY: xhcp
xhcp:
	make -C $@;

.PHONY: xrouted
xrouted:
	make -C $@;

.PHONY: nameserver_clean
nameserver_clean:
	make -C nameserver clean;

.PHONY: xhcp_clean
xhcp_clean:
	make -C xhcp clean;

.PHONY: xroute_clean
xrouted_clean:
	make -C xrouted clean;

