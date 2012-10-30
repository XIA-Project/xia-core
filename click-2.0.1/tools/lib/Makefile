SHELL = /bin/bash


top_srcdir = ../..
srcdir = .
top_builddir = ../..
subdir = tools/lib
conf_auxdir = $(top_srcdir)

prefix = /usr/local
bindir = /usr/local/bin
libdir = /usr/local/lib
includedir = /usr/local/include
toolincludedir = $(includedir)/clicktool
HOST_TOOLS = host

VPATH = .:$(top_srcdir)/$(subdir):$(top_srcdir)/tools/lib:$(top_srcdir)/lib

CC = gcc
CXX = g++
AR_CREATE = ar cru
RANLIB = ranlib
BUILD_CC = 
BUILD_CXX = 
BUILD_AR_CREATE =  cru
BUILD_RANLIB = 
INSTALL = /usr/bin/install -c
INSTALL_IF_CHANGED = $(INSTALL) -C
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_DATA_IF_CHANGED = $(INSTALL_IF_CHANGED) -m 644
mkinstalldirs = $(conf_auxdir)/mkinstalldirs

ifeq ($(V),1)
ccompile = $(COMPILE) $(1)
build_ccompile = $(BUILD_COMPILE) $(1)
cxxcompile = $(CXXCOMPILE) $(1)
build_cxxcompile = $(BUILD_CXXCOMPILE) $(1)
x_verbose_cmd = $(1) $(3)
verbose_cmd = $(1) $(3)
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
build_ccompile = @/bin/echo ' ' $(2) $< && $(BUILD_COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
build_cxxcompile = @/bin/echo ' ' $(2) $< && $(BUILD_CXXCOMPILE) $(1)
x_verbose_cmd = $(if $(2),/bin/echo ' ' $(2) $(3) &&,) $(1) $(3)
verbose_cmd = @$(x_verbose_cmd)
endif

.SUFFIXES:
.SUFFIXES: .S .c .cc .o .bo .s

.c.o:
	$(call ccompile,-c $< -o $@,CC)
	@-/bin/mv $*.d $*.hd 2>/dev/null
.s.o:
	$(call ccompile,-c $< -o $@,ASM)
	@-/bin/mv $*.d $*.hd 2>/dev/null
.S.o:
	$(call ccompile,-c $< -o $@,ASM)
	@-/bin/mv $*.d $*.hd 2>/dev/null
.cc.o:
	$(call cxxcompile,-c $< -o $@,CXX)
	@-/bin/mv $*.d $*.hd 2>/dev/null
.c.bo:
	$(call build_ccompile,-c $< -o $@,BUILDCC)
	@-test -f $*.d && sed 's/\.o:/\.bo:/' < $*.d > $*.bd && /bin/rm -f $*.d
.cc.bo:
	$(call build_cxxcompile,-c $< -o $@,BUILDCXX)
	@-test -f $*.d && sed 's/\.o:/\.bo:/' < $*.d > $*.bd && /bin/rm -f $*.d


OBJS = string.o straccum.o glue.o \
	bitvector.o vectorv.o hashallocator.o \
	ipaddress.o etheraddress.o \
	timestamp.o error.o \
	elementt.o eclasst.o routert.o runparse.o variableenv.o \
	landmarkt.o lexert.o lexertinfo.o driver.o \
	confparse.o args.o archive.o processingt.o etraits.o elementmap.o \
	userutils.o md5.o toolutils.o clp.o \
	xid.o xiapath.o xiaheader.o xiaextheader.o xiacontentheader.o \
	 ip6address.o 
BUILDOBJS = $(patsubst %.o,%.bo,$(OBJS))

CPPFLAGS =  -I/home/xia-user/xia-upgrade-click/xia-core/io_engine/include -D__MTCLICK__ -DCLICK_TOOL
CFLAGS = -g -O2 -W -Wall
CXXFLAGS = -g -O2
DEPCFLAGS = -MD -MP

DEFS = -DHAVE_CONFIG_H
INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(srcdir)
LDFLAGS = -L/home/xia-user/xia-upgrade-click/xia-core/io_engine/lib

CXXCOMPILE = $(CXX) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS)
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS)
BUILD_CXXCOMPILE = $(BUILD_CXX) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS)
BUILD_COMPILE = $(BUILD_CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS)

ifeq ($(HOST_TOOLS),mixed)
all: libclicktool.a libclicktool_build.a
endif
ifeq ($(HOST_TOOLS),build)
all: libclicktool_build.a
endif
ifeq ($(HOST_TOOLS),host)
all: libclicktool.a
endif

libclicktool.a: Makefile $(OBJS)
	$(call verbose_cmd,$(AR_CREATE) libclicktool.a $(OBJS),AR $@)
	$(call verbose_cmd,$(RANLIB) libclicktool.a,RANLIB $@)
libclicktool_build.a: Makefile $(BUILDOBJS)
	$(call verbose_cmd,$(BUILD_AR_CREATE) libclicktool_build.a $(BUILDOBJS),AR $@)
	$(call verbose_cmd,$(BUILD_RANLIB) libclicktool_build.a,RANLIB $@)

Makefile: $(srcdir)/Makefile.in
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_ELEMLISTS=no CONFIG_HEADERS= $(SHELL) ./config.status

DEPFILES := $(wildcard *.hd *.bd)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

install: install-local
install-lib install-local: libclicktool.a install-include
	$(call verbose_cmd,$(mkinstalldirs) $(DESTDIR)$(libdir))
	$(call verbose_cmd,$(INSTALL) libclicktool.a,INSTALL,$(DESTDIR)$(libdir)/libclicktool.a)
install-include:
	$(call verbose_cmd,$(mkinstalldirs) $(DESTDIR)$(toolincludedir))
	$(call verbose_cmd,$(INSTALL_DATA_IF_CHANGED) $(srcdir)/*.hh,INSTALLHEADERS,$(DESTDIR)$(toolincludedir))
uninstall:
	/bin/rm -f $(DESTDIR)$(libdir)/libclicktool.a
	cd $(srcdir); for i in *.hh; do /bin/rm -f $(DESTDIR)$(toolincludedir)/$$i; done

clean:
	rm -f *.d *.hd *.bd *.o *.bo libclicktool.a libclicktool_build.a
distclean: clean
	-rm -f Makefile

.PHONY: all clean distclean \
	install install-lib install-local install-include uninstall
