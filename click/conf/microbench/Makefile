GENERATED=ip_packetforward.click \
	  xia_packetforward_no_fallback.click \
	  xia_packetforward_fallback1.click \
	  xia_packetforward_fallback2.click \
	  xia_packetforward_update.click \
	  xia_packetforward_content_request_miss.click \
	  xia_packetforward_content_request_hit.click \
	  xia_packetforward_content_response.click

all: $(GENERATED)

%.click: %.pre ../xia_router_template.click packetforward_common.click ip_packetforward_common.click xia_packetforward_common.click
	../preprocess.rb $<

clean:
	$(RM) $(GENERATED)
	#$(RM) *_oprof
	#$(RM) *_timing
