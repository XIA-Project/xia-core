SHELL = /bin/bash


top_srcdir = ../..
srcdir = .
top_builddir = ../..
subdir = tools/click2xml
conf_auxdir = $(top_srcdir)

prefix = /usr/local
bindir = /usr/local/bin
HOST_TOOLS = host

VPATH = .:$(top_srcdir)/$(subdir):$(top_srcdir)/tools/lib:$(top_srcdir)/include

ifeq ($(HOST_TOOLS),build)
CC = 
CXX = 
LIBCLICKTOOL = libclicktool_build.a
DL_LIBS = 
else
CC = gcc
CXX = g++
LIBCLICKTOOL = libclicktool.a
DL_LIBS = -ldl
endif
INSTALL = /usr/bin/install -c
mkinstalldirs = $(conf_auxdir)/mkinstalldirs

ifeq ($(V),1)
ccompile = $(COMPILE) $(1)
cxxcompile = $(CXXCOMPILE) $(1)
cxxlink = $(CXXLINK) $(1)
x_verbose_cmd = $(1) $(3)
verbose_cmd = $(1) $(3)
else
ccompile = @/bin/echo ' ' $(2) $< && $(COMPILE) $(1)
cxxcompile = @/bin/echo ' ' $(2) $< && $(CXXCOMPILE) $(1)
cxxlink = @/bin/echo ' ' $(2) $@ && $(CXXLINK) $(1)
x_verbose_cmd = $(if $(2),/bin/echo ' ' $(2) $(3) &&,) $(1) $(3)
verbose_cmd = @$(x_verbose_cmd)
endif

.SUFFIXES:
.SUFFIXES: .S .c .cc .o .s

.c.o:
	$(call ccompile,-c $< -o $@,CC)
.s.o:
	$(call ccompile,-c $< -o $@,ASM)
.S.o:
	$(call ccompile,-c $< -o $@,ASM)
.cc.o:
	$(call cxxcompile,-c $< -o $@,CXX)


CLICK2XML_OBJS = click2xml.o
XML2CLICK_OBJS = xml2click.o

CPPFLAGS =  -I/home/xia-user/xia-upgrade-click/xia-core/io_engine/include -D__MTCLICK__ -DCLICK_TOOL
CFLAGS = -g -O2 -W -Wall
CXXFLAGS = -g -O2
DEPCFLAGS = -MD -MP

DEFS = -DHAVE_CONFIG_H
INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(top_srcdir)/tools/lib -I$(srcdir) 
LDFLAGS = -L/home/xia-user/xia-upgrade-click/xia-core/io_engine/lib
LIBS =  -lrt   -lexpat $(DL_LIBS)

CXXCOMPILE = $(CXX) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS)
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@

all: $(LIBCLICKTOOL) all-local
all-local: click2xml xml2click

$(LIBCLICKTOOL):
	@cd ../lib; $(MAKE) $(LIBCLICKTOOL)

click2xml: Makefile $(CLICK2XML_OBJS) ../lib/$(LIBCLICKTOOL)
	$(call cxxlink,-rdynamic $(CLICK2XML_OBJS) ../lib/$(LIBCLICKTOOL) $(LIBS),LINK)

xml2click: Makefile $(XML2CLICK_OBJS) ../lib/$(LIBCLICKTOOL)
	$(call cxxlink,-rdynamic $(XML2CLICK_OBJS) ../lib/$(LIBCLICKTOOL) $(LIBS),LINK)

Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_ELEMLISTS=no CONFIG_HEADERS= $(SHELL) ./config.status

DEPFILES := $(wildcard *.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

install: $(LIBCLICKTOOL) install-local
install-local: all-local
	$(call verbose_cmd,$(mkinstalldirs) $(DESTDIR)$(bindir))
	$(call verbose_cmd,$(INSTALL) click2xml,INSTALL,$(DESTDIR)$(bindir)/click2xml)
	@test -z "xml2click" || ( $(call x_verbose_cmd,$(INSTALL) xml2click,INSTALL,$(DESTDIR)$(bindir)/xml2click) )
uninstall:
	/bin/rm -f $(DESTDIR)$(bindir)/click2xml
	test -z "xml2click" || /bin/rm -f $(DESTDIR)$(bindir)/xml2click

clean:
	rm -f *.d *.o click2xml xml2click
distclean: clean
	-rm -f Makefile

.PHONY: all all-local clean distclean \
	install install-local uninstall $(LIBCLICKTOOL)
