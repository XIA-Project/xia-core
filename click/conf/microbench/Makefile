GENERATED=xia_packetforward.click

all: $(GENERATED)

%.click: %.pre ../xia_router_template.click
	#$(RM) -f $@
	../preprocess.rb $<

