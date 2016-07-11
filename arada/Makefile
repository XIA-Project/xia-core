all:
ifneq ($(wildcard /opt/buildroot-2013.11/.),) # no wave
	make -C waveserver
else
	@echo "Did not find WAVE directory"
endif

static: all

clean:
	make -C waveserver clean