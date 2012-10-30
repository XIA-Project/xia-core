# Warning: this file must be usable by regular make
# (unlike the Makefiles in subdirectories).

SHELL = /bin/bash


PACKAGE = click
VERSION = 2.0.1

top_srcdir = ../..
srcdir = .
top_builddir = ../..
subdir = etc/libclick
conf_auxdir = $(top_srcdir)

AUTOCONF = autoconf
ACLOCAL = aclocal -I m4
PERL = perl
INSTALL = /usr/bin/install -c
INSTALL_IF_CHANGED = $(INSTALL) -C
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_DATA_IF_CHANGED = $(INSTALL_IF_CHANGED) -m 644
mkinstalldirs = $(conf_auxdir)/mkinstalldirs

prefix = /usr/local
exec_prefix = ${prefix}
bindir = /usr/local/bin

all:
	@echo This Makefile does not build anything.
	@echo Its only important target is '`make dist'"'".

Makefile: $(top_builddir)/config.status $(srcdir)/Makefile.in
	cd $(top_builddir) && \
	  CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status

distdir = libclick-$(VERSION)
top_distdir = $(distdir)

clean:
	-rm -rf $(distdir) $(distdir).tar.gz
distclean: clean
	-rm -f Makefile

dist: $(distdir).tar.gz
	-rm -rf $(distdir)
$(distdir).tar.gz: distdir
	tar czf $(distdir).tar.gz $(distdir)
distdir: always
	@if test `grep 'CLICK_VERSION=' $(top_srcdir)/configure.in` != `grep 'CLICK_VERSION=' $(srcdir)/lc-configure.in`; then \
	  echo 'ERROR: Bad libclick CLICK_VERSION!'; exit 1; \
	fi
	-rm -rf $(distdir)
	mkdir $(distdir)
	@-chmod 777 $(distdir)
	@echo Copying library, documentation, configuration, and driver files...
	@echo DISTFILES >>$(distdir)/DISTFILES
	@for file in `cat $(srcdir)/DISTFILES`; do \
	  d=$(top_srcdir); \
	  sf=`echo "$$file:$$file:" | sed 's/^\([^:]*\):.*/\1/'`; \
	  df=`echo "$$file:$$file:" | sed 's/^[^:]*:\([^:]*\):.*/\1/'`; \
	  if test -d "$$d/$$sf"; then \
	    mkdir $(distdir)/$$df; \
	    chmod 777 $(distdir)/$$df; \
	  else \
	    test -f "$(distdir)/$$df" \
	    || ln $$d/$$sf $(distdir)/$$df 2>/dev/null \
	    || cp -p $$d/$$sf $(distdir)/$$df \
	    || echo "Could not copy $$d/$$sf!" 1>&2; \
	  fi; \
	  echo $$df >>$(distdir)/DISTFILES; \
	done
	cd $(distdir) && $(PERL) -pi -e 's/enable_linuxmodule=yes/enable_linuxmodule=no/;' configure.in
	cd $(distdir) && $(ACLOCAL) && $(AUTOCONF)
	cd $(distdir) && rm -rf autom4te*
	echo configure >>$(distdir)/DISTFILES

always:
	@:

.PHONY: all dist distdir always
