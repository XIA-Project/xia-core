GENERATED=ip_packetforward.click \
	  xia_packetforward_fallback0.click \
	  xia_packetforward_fallback1.click \
	  xia_packetforward_fallback2.click \
	  xia_packetforward_fallback3.click \
	  xia_packetforward_viapoint.click \
	  xia_packetforward_cid_req_miss.click \
	  xia_packetforward_cid_req_hit.click \
	  xia_packetforward_cid_rep.click \
	  xia_tablesize_ad.click \
	  xia_tablesize_cid.click

all: $(GENERATED)

%.click: %.pre ../xia_router_template.inc common.inc
	../preprocess.rb $<

clean:
	$(RM) $(GENERATED)
	#$(RM) *_perf_*
	#$(RM) *_timing_*
