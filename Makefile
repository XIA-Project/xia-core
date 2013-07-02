# list of top level directories that need to be built
MAKEDIRS=click api daemons applications

# make sure we run clean in anything we built in
CLEANDIRS=$(addsuffix .build, $(MAKEDIRS))

# list of directories with automated tests that should be run
TESTDIRS=$(addsuffix .test, api)

#set num of procs for parallel builds
NPROCS=$(shell grep -c ^processor /proc/cpuinfo)

.PHONY: all config clean test $(MAKEDIRS) $(CLEANDIRS) $(TESTDIRS)

NDKFLAGS=NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk APP_STL=gnustl_static APP_PLATFORM=android-9


#### BUILD RULES
#always make sure we have configured before building the sub projects
all: config $(MAKEDIRS)

android:
	ndk-build $(NDKFLAGS)

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
	ndk-build $(NDKFLAGS) clean
	-@rm -fr libs obj
	-@rm click/.configured
	-@rm xia.mk

$(CLEANDIRS):
	-make -C $(basename $@) clean



#### TEST RULES
test: $(TESTDIRS)

$(TESTDIRS):
	make -C $(basename $@) test
