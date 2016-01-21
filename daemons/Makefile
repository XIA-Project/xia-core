TARGETS=nameserver xhcp xrouted rendezvous gateway scionxrouted
CLEAN=$(addsuffix .build, $(TARGETS))

.PHONY: all clean $(TARGETS)

all: $(TARGETS)

static: all

$(TARGETS):
	make -C $@

clean: $(CLEAN)

$(CLEAN):
	-make -C $(basename $@) clean

