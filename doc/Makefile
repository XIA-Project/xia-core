
.PHONY: all html man clean

MANDIRS="xsocket xcache dagaddr/cpp"
HTMLDIRS="xsocket xcache dagaddr/cpp include"

doc: html man apps

apps:
	make -C ../bin doc
html:
	cd ../api && DOHTML=YES DIRS=$(HTMLDIRS) doxygen ../doc/api.cfg

man:
	cd ../api && DOMAN=YES DIRS=$(MANDIRS) doxygen ../doc/api.cfg

clean:
	@-rm -rf html
	@-rm -rf man
