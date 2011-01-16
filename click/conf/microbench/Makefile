GENERATED=xia_packetforward.click ip_packetforward.click xia_packetforward_fallback1.click xia_packetforward_fallback2.click \
	  xia_packetforward_update.click 

all: $(GENERATED)

%.click: %.pre ../xia_router_template.click
	#$(RM) -f $@
	../preprocess.rb $<

