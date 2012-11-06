all: nameserver xhcp xroute

clean: nameserver_clean xhcp_clean xroute_clean

.PHONY: nameserver
nameserver:
	make -C $@;

.PHONY: xhcp
xhcp:
	make -C $@;

.PHONY: xroute
xroute:
	make -C $@;

.PHONY: nameserver_clean
nameserver_clean:
	make -C nameserver clean;

.PHONY: xhcp_clean
xhcp_clean:
	make -C xhcp clean;

.PHONY: xroute_clean
xroute_clean:
	make -C xroute clean;

