/*
 * inotest.{cc,hh} -- regression test element for ClickIno
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "inotest.hh"
#include <click/ino.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#if CLICK_USERLEVEL
# define DT_REG 0
# define DT_DIR 1
# include "../../lib/ino.cc"
#endif
CLICK_DECLS

InoTest::InoTest()
{
}

InoTest::~InoTest()
{
}

namespace {
struct InoTestFilldir {
    int arg;
    String name;
    ErrorHandler *errh;
    InoTestFilldir(int a, const String &n, ErrorHandler *e)
	: arg(a), name(n), errh(e) {
    }
};

bool ino_test_filldir(const char *name, int name_len, ino_t ino, int dirtype,
		      uint32_t, void *user_data)
{
    InoTestFilldir *itf = static_cast<InoTestFilldir *>(user_data);
    itf->errh->message("argument %d: LS %s: %.*s %x %c",
		       itf->arg, itf->name.c_str(), name_len, name,
		       (unsigned) ino, dirtype == DT_DIR ? 'd' : 'f');
    return true;
}
}

int
InoTest::initialize(ErrorHandler *errh)
{
    int before = errh->nerrors();

    ClickIno ino;
    ino.initialize();
    ino.prepare(router(), 1);

    Vector<String> conf;
    cp_argvec(configuration(), conf);
    for (int arg = 0; arg < conf.size(); ++arg) {
	Vector<String> w;
	cp_spacevec(conf[arg], w);
	if (w.size() == 1)
	    w.push_back("");
	if (w.size() > 2)
	    errh->error("argument %d: syntax error", arg);
	if (w.size() != 2)
	    continue;

	ino_t ii = INO_GLOBALDIR;
	String dname = w[1];
	while (dname) {
	    if (INO_ISHANDLER(ii)) {
		errh->message("argument %d: %s: not a directory", arg, w[1].substring(w[1].begin(), dname.begin()).c_str());
		ii = 0;
		break;
	    }

	    const char *x = find(dname.begin(), dname.end(), '/');
	    String dcomp = dname.substring(dname.begin(), x);
	    ii = ino.lookup(ii, dcomp);
	    if (!ii) {
		errh->message("argument %d: %s: no such file", arg, w[1].substring(w[1].begin(), x).c_str());
		break;
	    }

	    dname = (x == dname.end() ? String() : dname.substring(x + 1, dname.end()));
	}

	if (!ii)
	    continue;

	if (w[0] == "INO")
	    errh->message("argument %d: INO %s %x", arg, w[1].c_str(),
			  (unsigned) ii);
	else if (w[0] == "NLINK") {
	    if (!INO_ISHANDLER(ii))
		errh->error("argument %d: %s: not a directory", arg, w[1].c_str());
	    else
		errh->message("argument %d: NLINK %s %d", arg, w[1].c_str(), ino.nlink(ii));
	} else if (w[0] == "LS") {
	    if (!INO_ISHANDLER(ii))
		errh->error("argument %d: %s: not a directory", arg, w[1].c_str());
	    else {
		InoTestFilldir itf(arg, w[1], errh);
		uint32_t f_pos = 0;
		ino.readdir(ii, f_pos, ino_test_filldir, &itf);
	    }
	} else
	    errh->error("argument %d: syntax error", arg);
    }

    if (errh->nerrors() == before)
	errh->message("All tests pass!");
    return (errh->nerrors() == before ? 0 : -1);
}

EXPORT_ELEMENT(InoTest)
CLICK_ENDDECLS
