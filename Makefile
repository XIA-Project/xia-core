ifneq ($(wildcard xia.mk),)
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

# generate the click makefile optimized for XIA
click/Makefile: click/Makefile.in xia.mk
	cd click; CXXFLAGS="$(CLICKFLAGS)" CFLAGS="$(CLICKFLAGS)" ./configure \
				--enable-user-multithread \
				--enable-warp9     \
				--enable-userlevel \
	 			--disable-analysis \
				--disable-tcpudp   \
				--disable-tools    \
				--disable-test     \
				--disable-app      \
				--disable-aqm      \
				--disable-simple   \
				--disable-linuxmodule


# treat click special since we want multi-proc compiles
click: click/Makefile
	CXXFLAGS="$(CLICKFLAGS)" CFLAGS="$(CLICKFLAGS)" make -j$(NPROCS) -C $@

# rules for all of the other directories
$(filter-out click, $(MAKEDIRS)):
	make -C $@ $(STATIC)


#### CONFIG RULES
# add other configuration targets here as needed
config: xia.mk click/Makefile

# creates xia.mk
xia.mk: configure
	@./configure
	@echo "xia.mk generated (ignore the error below & rerun make)"
	@false
	
xia.env:
	echo "export CC=\"${CC}\"" > xia.env
	echo "export LD=\"${LD}\"" >> xia.env
	echo "export LDFLAGS=\"-lstdc++ ${ENV_LDFLAGS}\"" >> xia.env
	echo "export CFLAGS=\"${CFLAGS}\"" >> xia.env
	echo "export LD_LIBRARY_PATH=\"${ENV_LD_LIBRARY_PATH}\"" >> xia.env

#### CLEAN RULES
clean: $(CLEANDIRS)
	-@rm click/Makefile
	-@rm xia.mk

$(CLEANDIRS):
	-make -C $(basename $@) clean

#### print out the value of a variable such as CFLAGS
dump-% :
	@echo $* = $($*)

#### TEST RULES
test: $(TESTDIRS)

$(TESTDIRS):
	make -C $(basename $@) test
