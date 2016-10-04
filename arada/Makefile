all:
ifneq ($(wildcard /opt/buildroot-2013.11/.),) # no wave
	make -C waveserver
	make -C waveserverip
	make -C waveserveripudp
	make -C pdrtestip
else
	@echo "Did not find WAVE directory"
endif

static: all

clean:
	make -C waveserver clean
	make -C waveserverip clean
	make -C waveserveripudp clean
	make -C pdrtestip clean
