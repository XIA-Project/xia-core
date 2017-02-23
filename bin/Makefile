ifneq ($(wildcard ../xia.mk),)
include ../xia.mk
endif

MANDIR=$(XIADIR)/doc/man/man1
MANUAL="XIA system utilities"
SOURCE="Carnegie Mellon University"

UTILITIES=     \
	cachectl   \
	psx        \
	xarp       \
	xdag       \
	xdebug     \
	xdocs      \
	xianet     \
	xinfo      \
	xlog       \
	xmakehosts \
	xmodified  \
	xnetstat   \
	xroute     \
	xtop       \
	xwrap


MANPAGES=$(addprefix $(MANDIR)/, $(addsuffix .1, $(UTILITIES)))

.PHONY: all doc clean


#all: doc

doc: $(MANDIR) $(MANPAGES)

$(MANDIR)/cachectl.1: ./cachectl
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "enable/disable caching" ./$< -o$@

$(MANDIR)/psx.1: ./psx
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "list process ids" ./$< -o$@

$(MANDIR)/xarp.1: ./xarp
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "manage XIA ARP table" ./$< -o$@

$(MANDIR)/xdag.1: ./xdag
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "display system's DAG" ./$< -o$@

$(MANDIR)/xdebug.1: ./xdebug
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "attach gdb to a running process" ./$< -o$@

$(MANDIR)/xianet.1: ./xianet
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "control XIA node" ./$< -o $@

$(MANDIR)/xinfo.1: ./xinfo
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "dump XIA system information" ./$< -o$@

$(MANDIR)/xlog.1: ./xlog
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "manage click log level" ./$< -o$@

$(MANDIR)/xmakehosts.1: ./xmakehosts
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "create etc/hosts.xia" ./$< -o$@

$(MANDIR)/xmodified.1: ./xmodified
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "copy modified XIA source files to another host" ./$< -o$@

$(MANDIR)/xnetstat.1: ./xnetstat
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "manage socket table" ./$< -o$@

$(MANDIR)/xroute.1: ./xroute
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "manage XIA route tables" ./$< -o$@

$(MANDIR)/xtop.1: ./xtop
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "display XIA system processes in top" ./$< -o$@

$(MANDIR)/xwrap.1: ./xwrap
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "run application using the XIA interposition library" ./$< -o$@

$(MANDIR)/xdocs.1: ./xdocs
	help2man -N -m $(MANUAL) -S $(SOURCE) -n "view online documentation" ./$< -o$@

$(MANDIR):
	@[ ! -d $(MANDIR) ] && mkdir $(MANDIR)


clean:
	@-rm -rf $(MANDIR)
