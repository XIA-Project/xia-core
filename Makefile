# list of top level directories that need to be built
MAKEDIRS=click api daemons applications

# make sure we run clean in anything we built in
CLEANDIRS=$(addsuffix .build, $(MAKEDIRS))

# list of directories with automated tests that should be run
TESTDIRS=$(addsuffix .test, api)

#set number of procs == (2 * actual processors)
NPROCS=$(shell echo `grep -c ^processor /proc/cpuinfo`\*2 | bc)

.PHONY: all config clean test $(MAKEDIRS) $(CLEANDIRS) $(TESTDIRS)



#### BUILD RULES
#always make sure we have configured before building the sub projects
all: config $(MAKEDIRS)

# treat click special since we want multi-proc compiles
click:
	make -j$(NPROCS) -C $@

$(filter-out click, $(MAKEDIRS)):
	make -C $@



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
