ifneq ($(wildcard ../xia.mk),)
include ../xia.mk
endif

MANDIR=$(XIADIR)/doc/man/man1
MANUAL="XIA system utilities"
SOURCE="Carnegie Mellon University"
CMDLINE=--no-discard-stderr -N -m $(MANUAL) -S $(SOURCE)

#
# if needed
# command to generate online man pages, currently not automated
# man2html xtrace.1 | pandoc -f html -t markdown > ~/osx/foo.md
#


UTILITIES=     \
	addrouter  \
	addtopeer  \
	cachectl   \
	psx        \
	xarp       \
	xdag       \
	xdebug     \
	xdocs      \
	xia        \
	xia-config \
	xiaman     \
	xinfo      \
	xlog       \
	xmakehosts \
	xmodified  \
	xnetstat   \
	xping      \
	xput       \
	xroute     \
	xtop       \
	xtrace     \
	xtraceroute\
	xwrap


MANPAGES=$(addprefix $(MANDIR)/, $(addsuffix .1, $(UTILITIES)))

.PHONY: all doc clean


#all: doc

doc: $(MANDIR) $(MANPAGES)

$(MANDIR)/xia.1: ./xia
	echo "[start]" > help.tmp
	./xia start -h >> help.tmp
	echo "[stop]" >> help.tmp
	./xia stop -h >> help.tmp
	echo "[status]" >> help.tmp
	./xia status -h >> help.tmp
	help2man $(CMDLINE) -i help.tmp -n "manage an XIA node" ./$< -o$@
	rm help.tmp

$(MANDIR)/xia-config.1: ./xia-config
	echo "[info]" > help.tmp
	./xia-config info -h >> help.tmp
	echo "[clean]" >> help.tmp
	./xia-config clean -h >> help.tmp
	echo "[host]" >> help.tmp
	./xia-config host -h >> help.tmp
	echo "[router]" >> help.tmp
	./xia-config router -h >> help.tmp
	echo "[controller]" >> help.tmp
	./xia-config controller -h >> help.tmp
	help2man $(CMDLINE) -i help.tmp -n "configure an XIA node" ./$< -o$@
	rm help.tmp

$(MANDIR)/addrouter.1: ./addrouter
	help2man $(CMDLINE) -n "add a router to the AD" ./$< -o$@

$(MANDIR)/addtopeer.1: ./addtopeer
	help2man $(CMDLINE) -n "join a neighbor AD" ./$< -o$@

$(MANDIR)/cachectl.1: ./cachectl
	help2man $(CMDLINE) -n "enable/disable caching" ./$< -o$@

$(MANDIR)/psx.1: ./psx
	help2man $(CMDLINE) -n "list process ids" ./$< -o$@

$(MANDIR)/xarp.1: ./xarp
	help2man $(CMDLINE) -n "manage XIA ARP table" ./$< -o$@

$(MANDIR)/xdag.1: ./xdag
	help2man $(CMDLINE) -n "display system's DAG" ./$< -o$@

$(MANDIR)/xdebug.1: ./xdebug
	help2man $(CMDLINE) -n "attach gdb to a running process" ./$< -o$@

$(MANDIR)/xiaman.1: ./xiaman
	help2man $(CMDLINE) -n "display XIA man pages" ./$< -o $@

$(MANDIR)/xinfo.1: ./xinfo
	help2man $(CMDLINE) -n "dump XIA system information" ./$< -o$@

$(MANDIR)/xlog.1: ./xlog
	help2man $(CMDLINE) -n "manage click log level" ./$< -o$@

$(MANDIR)/xmakehosts.1: ./xmakehosts
	help2man $(CMDLINE) -n "create etc/hosts.xia" ./$< -o$@

$(MANDIR)/xmodified.1: ./xmodified
	help2man $(CMDLINE) -n "copy modified XIA source files to other hosts" ./$< -o$@

$(MANDIR)/xnetstat.1: ./xnetstat
	help2man $(CMDLINE) -n "manage socket table" ./$< -o$@

$(MANDIR)/xping.1: ./xping
	help2man $(CMDLINE) -n "send XCMP ECHO_REQUEST to network hosts" ./$< -o$@

$(MANDIR)/xput.1: ./xput
	help2man $(CMDLINE) -n "copy specified file to other hosts" ./$< -o$@

$(MANDIR)/xroute.1: ./xroute
	help2man $(CMDLINE) -n "manage XIA route tables" ./$< -o$@

$(MANDIR)/xtop.1: ./xtop
	help2man $(CMDLINE) -n "display XIA system processes in top" ./$< -o$@

$(MANDIR)/xtraceroute.1: ./xtraceroute
	help2man $(CMDLINE) -n "print the route packets trace to network host" ./$< -o$@

$(MANDIR)/xwrap.1: ./xwrap
	help2man $(CMDLINE) -n "run application using the XIA interposition library" ./$< -o$@

$(MANDIR)/xdocs.1: ./xdocs
	help2man $(CMDLINE) -n "view online documentation" ./$< -o$@

$(MANDIR)/xtrace.1: ./xtrace
	help2man $(CMDLINE) -n "manage packet tracing" ./$< -o$@

$(MANDIR):
	@[ ! -d $(MANDIR) ] && mkdir $(MANDIR)


clean:
	@-rm -rf $(MANDIR)
