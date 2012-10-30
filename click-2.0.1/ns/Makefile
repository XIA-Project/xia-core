## The following line supports click-mkmindriver and should not be changed.
## Click ns driver Makefile ##
SHELL = /bin/bash

top_srcdir := ..
srcdir := .
top_builddir := ..
subdir := ns
conf_auxdir := $(top_srcdir)

prefix = /usr/local
exec_prefix = ${prefix}
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
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(DEPCFLAGS) $(1)
ccompile_nodep = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(DEPCFLAGS) $(1)
cxxcompile_nodep = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
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
	$(call cxxcompile,-S $< -o $@,CXX -S)
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
	$(EXTRA_DRIVER_OBJS)

EXTRA_DRIVER_OBJS = ip6address.o ip6flowid.o ip6table.o 

LIBOBJS = $(GENERIC_OBJS) $(STD_ELEMENT_OBJS) clp.o
STD_ELEMENT_OBJS = addressinfo.o alignmentinfo.o \
	errorelement.o portinfo.o scheduleinfo.o
OBJS = $(ELEMENT_OBJS) $(ELEMENTSCONF).o nsclick.o

CPPFLAGS =  -I/home/xia-user/xia-upgrade-click/xia-core/io_engine/include -D__MTCLICK__ -DCLICK_NS
CFLAGS = -g -O2 -W -Wall -fPIC
CXXFLAGS = -g -O2 -fPIC

DEFS = -DHAVE_CONFIG_H
INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(srcdir) -I$(top_srcdir) 
LDFLAGS = -shared
LIBS =  `$(top_builddir)/click-buildtool --otherlibs` $(ELEMENT_LIBS)

CXXCOMPILE = $(CXX) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@

ifndef MINDRIVER
ELEMENTSCONF = elements
INSTALLLIBS = libnsclick.so
FINDELEMFLAGS = 
else
ELEMENTSCONF = elements_$(MINDRIVER)
INSTALLLIBS = libns$(MINDRIVER)click.so
endif

all: $(INSTALLLIBS)

ifneq ($(MAKECMDGOALS),clean)
-include $(ELEMENTSCONF).mk
endif

libnsclick.so: Makefile $(LIBOBJS) $(OBJS)
	$(CXXLINK) -shared $(LIBOBJS) $(OBJS)
libnsclick.dylib: Makefile $(LIBOBJS) $(OBJS)
	$(CXXLINK) -dynamiclib $(LIBOBJS) $(OBJS)
libnsclick.a: Makefile $(LIBOBJS) $(OBJS)
	$(AR_CREATE) libnsclick.a $(LIBOBJS) $(OBJS)
	$(RANLIB) libnsclick.a

nsclick-test: libnsclick.a nsclick-test.o
	$(CXXLD) $(CXXFLAGS) -L/home/xia-user/xia-upgrade-click/xia-core/io_engine/lib -o $@ nsclick-test.o libnsclick.a $(LIBS)

Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status

elemlist:
	@rm -f elements.conf
	@$(MAKE) elements.conf
elements.conf: $(top_builddir)/config.status $(top_builddir)/click-buildtool
	echo "ns userlevel  analysis app aqm ethernet icmp ip ip6 ipsec simple standard tcpudp test threads xia" | $(top_builddir)/click-buildtool findelem -r ns -p $(top_srcdir) -X $(srcdir)/elements.exclude $(FINDELEMFLAGS) > elements.conf
$(ELEMENTSCONF).mk: $(ELEMENTSCONF).conf $(top_builddir)/click-buildtool
	$(top_builddir)/click-buildtool elem2make -x "$(STD_ELEMENT_OBJS)" < $(ELEMENTSCONF).conf > $(ELEMENTSCONF).mk
$(ELEMENTSCONF).cc: $(ELEMENTSCONF).conf $(top_builddir)/click-buildtool
	$(top_builddir)/click-buildtool elem2export < $(ELEMENTSCONF).conf > $(ELEMENTSCONF).cc
	@rm -f $(ELEMENTSCONF).d

DEPFILES := $(wildcard *.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

install: $(INSTALLLIBS)
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	for i in $(INSTALLLIBS); do $(INSTALL) $$i $(DESTDIR)$(libdir)/$$i; done
install-include:
	@:
uninstall:
	for i in $(INSTALLLIBS); do rm -f $(DESTDIR)$(libdir)/$$i; done

clean:
	rm -f *.d *.o $(ELEMENTSCONF).mk $(ELEMENTSCONF).cc elements.conf libnsclick.a \
	$(INSTALLLIBS)
distclean: clean
	-rm -f Makefile

.PHONY: all clean distclean elemlist \
	install install-include uninstall
