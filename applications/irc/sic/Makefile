# sic - simple irc client

include config.mk

SRC = sic.c
OBJ = ${SRC:.c=.o}

all: options sic

options:
	@echo sic build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk util.c

sic: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f sic ${OBJ} sic-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p sic-${VERSION}
	@cp -R LICENSE Makefile README config.mk sic.1 sic.c util.c sic-${VERSION}
	@tar -cf sic-${VERSION}.tar sic-${VERSION}
	@gzip sic-${VERSION}.tar
	@rm -rf sic-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f sic ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/sic
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < sic.1 > ${DESTDIR}${MANPREFIX}/man1/sic.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/sic.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/sic
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/sic.1

.PHONY: all options clean dist install uninstall
