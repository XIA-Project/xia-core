## The following line supports click-mkmindriver and should not be changed.
## Click userlevel driver Makefile ##
SHELL = /bin/bash

top_srcdir := ..
srcdir := .
top_builddir := ..
subdir := userlevel
conf_auxdir := $(top_srcdir)

prefix = /usr/local
bindir = /usr/local/bin
sbindir = /usr/local/sbin
libdir = /usr/local/lib

VPATH = .:$(top_srcdir)/lib:$(top_srcdir)/$(subdir):$(top_srcdir)/elements/standard

CC = gcc
CPP = gcc -E
CXX = g++
CXXCPP = g++ -E
DEPCFLAGS = -MD -MP
AR_CREATE = ar cru
RANLIB = ranlib
INSTALL = /usr/bin/install -c
mkinstalldirs = $(conf_auxdir)/mkinstalldirs

ifeq ($(V),1)
ccompile = $(COMPILE) $(DEPCFLAGS) $(1)
ccompile_nodep = $(COMPILE) $(1)
cxxcompile = $(CXXCOMPILE) $(DEPCFLAGS) $(1)
cxxcompile_nodep = $(CXXCOMPILE) $(1)
cxxlink = $(CXXLINK) $(1)
x_verbose_cmd = $(1) $(3)
verbose_cmd = $(1) $(3)
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(DEPCFLAGS) $(1)
ccompile_nodep = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(DEPCFLAGS) $(1)
cxxcompile_nodep = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
cxxlink = @/bin/echo ' ' $(2) $@ && $(CXXLINK) $(1)
x_verbose_cmd = $(if $(2),/bin/echo ' ' $(2) $(3) &&,) $(1) $(3)
verbose_cmd = @$(x_verbose_cmd)
endif

.SUFFIXES:
.SUFFIXES: .S .c .cc .o .s .ii

.c.o:
	$(call ccompile,-c $< -o $@,CC)
.s.o:
	$(call ccompile,-c $< -o $@,ASM)
.S.o:
	$(call ccompile,-c $< -o $@,ASM)
.cc.o:
	$(call cxxcompile,-c $< -o $@,CXX)
.cc.s:
	$(call cxxcompile_nodep,-S $< -o $@,CXX -S)
.cc.ii:
	$(call cxxcompile_nodep,-E $< > $@,CXXCPP)


GENERIC_OBJS = string.o straccum.o nameinfo.o \
	bitvector.o vectorv.o templatei.o bighashmap_arena.o hashallocator.o \
	ipaddress.o ipflowid.o etheraddress.o \
	packet.o \
	error.o timestamp.o glue.o task.o timer.o atomic.o fromfile.o gaprate.o \
	element.o \
	confparse.o args.o variableenv.o lexer.o elemfilter.o routervisitor.o \
	routerthread.o router.o master.o timerset.o selectset.o handlercall.o notifier.o \
	integers.o md5.o crc32.o in_cksum.o iptable.o \
	archive.o userutils.o driver.o \
	xid.o xidpair.o xiapath.o xiaheader.o xiaextheader.o xiacontentheader.o xiatransportheader.o xia.pb.o \
	$(EXTRA_DRIVER_OBJS)

EXTRA_DRIVER_OBJS = ip6address.o ip6flowid.o ip6table.o 

LIBOBJS = $(GENERIC_OBJS) $(STD_ELEMENT_OBJS) clp.o exportstub.o
STD_ELEMENT_OBJS = addressinfo.o alignmentinfo.o \
	errorelement.o portinfo.o scheduleinfo.o \
	xiaxidinfo.o
OBJS = $(ELEMENT_OBJS) $(ELEMENTSCONF).o click.o

CPPFLAGS =  -I/home/xia-user/xia-upgrade-click/xia-core/io_engine/include -D__MTCLICK__ -DCLICK_USERLEVEL
CFLAGS = -g -O2 -W -Wall
CXXFLAGS = -g -O2

DEFS = -DHAVE_CONFIG_H
INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(srcdir) -I$(top_srcdir)  
LDFLAGS = -L/home/xia-user/xia-upgrade-click/xia-core/io_engine/lib
LIBS =  `$(top_builddir)/click-buildtool --otherlibs` $(ELEMENT_LIBS)

CXXCOMPILE = $(CXX) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@

ifndef MINDRIVER
DRIVER = click
ELEMENTSCONF = elements
INSTALLLIBS = libclick.a
FINDELEMFLAGS = 
else
DRIVER = $(MINDRIVER)click
ELEMENTSCONF = elements_$(MINDRIVER)
endif
INSTALLPROGS = $(DRIVER)

all: $(INSTALLPROGS) $(INSTALLLIBS)

xia.pb.cc: xia.proto
	protoc --cpp_out=. --python_out=../../web xia.proto

ifneq ($(MAKECMDGOALS),clean)
-include $(ELEMENTSCONF).mk
endif

$(DRIVER): Makefile libclick.a $(OBJS)
	$(call cxxlink,-rdynamic $(OBJS) libclick.a $(LIBS),LINK)

libclick.a: Makefile $(LIBOBJS)
	$(call verbose_cmd,$(AR_CREATE) libclick.a $(LIBOBJS),AR libclick.a)
	$(call verbose_cmd,$(RANLIB),RANLIB,libclick.a)

Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status

elemlist:
	@rm -f elements.conf
	@$(MAKE) elements.conf
elements.conf: $(top_builddir)/config.status $(top_builddir)/click-buildtool
	$(call verbose_cmd,echo "userlevel  analysis app aqm ethernet icmp ip ip6 ipsec simple standard tcpudp test threads xia" | $(top_builddir)/click-buildtool findelem -r userlevel -p $(top_srcdir) -X $(srcdir)/elements.exclude $(FINDELEMFLAGS) >,FINDELEMENTS,elements.conf)
$(ELEMENTSCONF).mk: $(ELEMENTSCONF).conf $(top_builddir)/click-buildtool
	$(call verbose_cmd,$(top_builddir)/click-buildtool elem2make -x "$(STD_ELEMENT_OBJS)" < $(ELEMENTSCONF).conf >,CREATE,$(ELEMENTSCONF).mk)
$(ELEMENTSCONF).cc: $(ELEMENTSCONF).conf $(top_builddir)/click-buildtool
	$(call verbose_cmd,$(top_builddir)/click-buildtool elem2export < $(ELEMENTSCONF).conf >,CREATE,$(ELEMENTSCONF).cc)
	@rm -f $(ELEMENTSCONF).d

DEPFILES := $(wildcard *.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

install: $(INSTALLPROGS) install-lib
	$(call verbose_cmd,$(mkinstalldirs) $(DESTDIR)$(bindir))
	@for i in $(INSTALLPROGS); do $(call x_verbose_cmd,$(INSTALL) $$i,INSTALL,$(DESTDIR)$(bindir)/$$i); done
install-include:
	@:
install-lib: $(INSTALLLIBS)
	$(call verbose_cmd,$(mkinstalldirs) $(DESTDIR)$(libdir))
	@for i in $(INSTALLLIBS); do $(call x_verbose_cmd,$(INSTALL) $$i,INSTALL,$(DESTDIR)$(libdir)/$$i); done
install-man:
	@:
uninstall:
	for i in $(INSTALLLIBS); do rm -f $(DESTDIR)$(libdir)/$$i; done
	for i in $(INSTALLPROGS); do rm -f $(DESTDIR)$(bindir)/$$i; done

clean:
	rm -f *.d *.o $(INSTALLPROGS) $(ELEMENTSCONF).mk $(ELEMENTSCONF).cc elements.conf libclick.a xia.pb.h xia.pb.cc
clean-lib:
	rm -f $(LIBOBJS) libclick.a
distclean: clean
	-rm -f Makefile

.PHONY: all clean clean-lib distclean elemlist \
	install install-include install-lib install-man uninstall
