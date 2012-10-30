## Click bsdmodule driver Makefile ##
SHELL = /bin/bash

srcdir := .
top_srcdir := ..
top_builddir := ..
subdir := bsdmodule

prefix = /usr/local
exec_prefix = ${prefix}
bindir = /usr/local/bin
sbindir = /usr/local/sbin
libdir = /usr/local/lib
freebsd_includedir = /usr/include
freebsd_srcdir = NONE

VPATH = .:$(top_srcdir)/lib:$(top_srcdir)/$(subdir)

CC = gcc
CPP = gcc -E
CXX = g++ -w  -W -Wall -fno-exceptions -fno-rtti -fpermissive
CXXCPP = g++ -E
DEPCFLAGS = -MD -MP
LD = ld
STRIP = strip
PERL = perl
INSTALL = /usr/bin/install -c
INSTALL_DATA = $(INSTALL) -m 644
mkinstalldirs = ../mkinstalldirs

target_cpu = x86_64

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

quiet_cmd_cxxcompile = CXX $(quiet_modtag) $(subst $(obj)/,,$@)
cmd_cxxcompile = $(CXXCOMPILE) -c -o $@ $<

quiet_cmd_ccompile = CC $(quiet_modtag) $(subst $(obj)/,,$@)
cmd_ccompile = $(COMPILE) -c -o $@ $<

.SUFFIXES:
.SUFFIXES: .S .c .cc .o .s .ii

.c.o:
	$(COMPILE) $(DEPCFLAGS) -c $<
.s.o:
	$(COMPILE) $(DEPCFLAGS) -c $<
.S.o:
	$(COMPILE) $(DEPCFLAGS) -c $<
.cc.o:
	$(CXXCOMPILE) $(DEPCFLAGS) -c $<
.cc.s:
	$(CXXCOMPILE) -S $<
.cc.ii:
	$(CXXCOMPILE) -E $< > $@


ifndef INSTALLOBJS
INSTALLOBJS = click.ko
endif

GENERIC_OBJS = string.o straccum.o nameinfo.o \
	bitvector.o vectorv.o templatei.o bighashmap_arena.o hashallocator.o \
	ipaddress.o ipflowid.o etheraddress.o \
	packet.o in_cksum.o \
	error.o timestamp.o glue.o task.o timer.o atomic.o gaprate.o \
	element.o \
	confparse.o args.o variableenv.o lexer.o elemfilter.o routervisitor.o \
	routerthread.o router.o master.o timerset.o handlercall.o notifier.o \
	integers.o crc32.o iptable.o \
	driver.o \
	$(EXTRA_DRIVER_OBJS)

BSDMODULE_OBJS = config.o sched.o module.o module_c.o \
	clickfs.o clickfs_vnops.o clickfs_tree.o clickfs_element.o

EXTRA_DRIVER_OBJS = ip6address.o ip6flowid.o ip6table.o 

OBJS = $(GENERIC_OBJS) $(ELEMENT_OBJS) $(BSDMODULE_OBJS) elements.o

#CPPFLAGS =  -I/home/xia-user/xia-upgrade-click/xia-core/io_engine/include -D__MTCLICK__ -DCLICK_BSDMODULE -DBSD_NETISRSCHED
CPPFLAGS =  -I/home/xia-user/xia-upgrade-click/xia-core/io_engine/include -D__MTCLICK__ -DCLICK_BSDMODULE

ifeq ($(target_cpu),x86_64)
CFLAGS =  -O2 -g -fPIC -fno-builtin
CXXFLAGS =  -O2 -g -fPIC -fno-builtin
else
CFLAGS =  -O2 -g -fno-builtin
CXXFLAGS =  -O2 -g -fno-builtin
endif

DEFS = -DHAVE_CONFIG_H -D_KERNEL
INCLUDES = -nostdinc -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(srcdir) -I$(top_srcdir) -I. -I$(freebsd_includedir)
LDFLAGS = -L/home/xia-user/xia-upgrade-click/xia-core/io_engine/lib
LIBS = 

CXXCOMPILE = $(CXX) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@

FINDELEMFLAGS = 

all: $(INSTALLOBJS)

ifneq ($(MAKECMDGOALS),clean)
-include elements.mk
endif

click.ko: Makefile vnode_if.h vnode_if_newproto.h vnode_if_typedef.h $(OBJS)
	$(LD) -Bshareable -o click.ko $(OBJS)

Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status

elemlist:
	@/bin/rm -f elements.conf
	@$(MAKE) elements.conf
elements.conf: $(top_builddir)/config.status $(top_builddir)/click-buildtool
	echo "bsdmodule  analysis app aqm ethernet icmp ip ip6 ipsec simple standard tcpudp test threads xia" | $(top_builddir)/click-buildtool findelem -r bsdmodule -p $(top_srcdir) -X $(srcdir)/elements.exclude $(FINDELEMFLAGS) > elements.conf
elements.mk: elements.conf $(top_builddir)/click-buildtool
	$(top_builddir)/click-buildtool elem2make < elements.conf > elements.mk
elements.cc: elements.conf $(top_builddir)/click-buildtool
	$(top_builddir)/click-buildtool elem2export < elements.conf > elements.cc
	@rm -f elements.d

vnode_if.h: $(freebsd_srcdir)/kern/vnode_if.src
	/usr/bin/awk -f $(freebsd_srcdir)/tools/vnode_if.awk $(freebsd_srcdir)/kern/vnode_if.src -h
vnode_if_newproto.h: $(freebsd_srcdir)/kern/vnode_if.src
	/usr/bin/awk -f $(freebsd_srcdir)/tools/vnode_if.awk $(freebsd_srcdir)/kern/vnode_if.src -p
vnode_if_typedef.h: $(freebsd_srcdir)/kern/vnode_if.src
	/usr/bin/awk -f $(freebsd_srcdir)/tools/vnode_if.awk $(freebsd_srcdir)/kern/vnode_if.src -q


DEPFILES := $(wildcard *.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

install: $(INSTALLOBJS)
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	for i in $(INSTALLOBJS); do $(INSTALL_DATA) $$i $(DESTDIR)$(libdir)/$$i; done
install-include:
	@:
install-man:
	@:
uninstall:
	for i in $(INSTALLOBJS); do /bin/rm -f $(DESTDIR)$(libdir)/$$i; done

clean:
	-rm -f *.d *.o click.ko elements.mk elements.cc elements.conf \
	vnode_if.h vnode_if_newproto.h vnode_if_typedef.h
distclean: clean
	-rm -f Makefile

.PHONY: all clean distclean elemlist \
	install install-include install-man uninstall
