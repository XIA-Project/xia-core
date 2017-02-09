
.PHONY: all html man clean

MANDIRS="xsocket xcache"
HTMLDIRS="xsocket xcache include"

doc: html man

html:
	cd ../api && DOHTML=YES DIRS=$(HTMLDIRS) doxygen ../doc/api.cfg

man:
	cd ../api && DOMAN=YES DIRS=$(MANDIRS) doxygen ../doc/api.cfg

clean:
	@-rm -rf html
	@-rm -rf man
