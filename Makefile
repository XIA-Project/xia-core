ifeq ($(wildcard xia.mk),)
$(error You must run configure first)
else
include xia.mk
endif

# list of top level directories that need to be built
MAKEDIRS=click api daemons applications tools

# make sure we run clean in anything we built in
CLEANDIRS=$(addsuffix .build, $(MAKEDIRS))

# list of directories with automated tests that should be run
TESTDIRS=$(addsuffix .test, api)

.PHONY: all config clean test $(MAKEDIRS) $(CLEANDIRS) $(TESTDIRS)


#### BUILD RULES
#always make sure we have configured before building the sub projects
all: config $(MAKEDIRS)

static:
	make all STATIC='static'

# treat click special since we want multi-proc compiles
click:
	make -j$(NPROCS) -C $@

$(filter-out click, $(MAKEDIRS)):
	make -C $@ $(STATIC)


#### CONFIG RULES
# add other configuration targets here as needed
config: xia.mk
	@cd click && ./conf_xia

xia.mk: configure
	@./configure


#### CLEAN RULES
clean: $(CLEANDIRS)
	-@rm click/.configured
	-@rm xia.mk

$(CLEANDIRS):
	-make -C $(basename $@) clean



#### TEST RULES
test: $(TESTDIRS)

$(TESTDIRS):
	make -C $(basename $@) test
