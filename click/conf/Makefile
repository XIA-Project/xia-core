GENERATED=xia_chain_topology.click

all: $(GENERATED) microbench

%.click: %.pre xia_router_template.click
	#$(RM) -f $@
	./preprocess.rb $<

.PHONY: microbench
microbench:
	@cd microbench && make

