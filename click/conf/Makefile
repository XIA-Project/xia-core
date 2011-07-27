GENERATED=xia_chain_topology.click

all: $(GENERATED) microbench

%.click: %.pre xia_router_template.inc
	./preprocess.rb $<

.PHONY: microbench
microbench:
	@cd microbench && make

clean:
	$(RM) $(GENERATED)
	@cd microbench && make clean

install:
