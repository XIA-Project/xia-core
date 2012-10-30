/* include/click/pathvars.h.  Generated from pathvars.h.in by configure.  */
/* Process this file with configure to produce pathvars.h. -*- mode: c -*- */
#ifndef CLICK_PATHVARS_H
#define CLICK_PATHVARS_H

/* Directory for binaries. */
#define CLICK_BINDIR "/usr/local/bin"

/* Directory for packages and kernel module. */
#define CLICK_LIBDIR "/usr/local/lib"

/* Directory for shared files. */
#define CLICK_DATADIR "/usr/local/share/click"

/* FreeBSD kernel include directory. */
#define FREEBSD_INCLUDEDIR "/usr/include"

/* Define if the BSD kernel module driver was compiled. */
/* #undef HAVE_BSDMODULE_DRIVER */

/* Define if the Click kernel module should provide clickfs. */
#define HAVE_CLICKFS 1

/* Define if the expat library is available. */
#define HAVE_EXPAT 1

/* Define if the Click linuxmodule is compiled for a 2.6 kernel. */
/* #undef HAVE_LINUXMODULE_2_6 */

/* Define if the Linux kernel module driver was compiled. */
/* #undef HAVE_LINUXMODULE_DRIVER */

/* Define if the user-level driver was compiled. */
#define HAVE_USERLEVEL_DRIVER 1

/* Directory containing Linux sources. */
#define LINUX_SRCDIR "NONE"

#endif
