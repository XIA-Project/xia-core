GENERATED=packet_forwarding_breakdown.pdf tablesize.pdf

all: $(GENERATED)

%.pdf: %.py common.py
	./$<
	cp $@ ../../../../../xia-doc/paper/fig

clean:
	rm -f $(GENERATED)
