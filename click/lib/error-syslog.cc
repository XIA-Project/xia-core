// -*- related-file-name: "../include/click/error.hh" -*-
/*
 * error.{cc,hh} -- flexible classes for error reporting
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001-2008 Eddie Kohler
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
#include <click/error.hh>
#include <click/straccum.hh>
#ifndef CLICK_TOOL
# include <click/element.hh>
#endif
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/timestamp.hh>
#include <click/confparse.hh>
#include <click/algorithm.hh>
#if CLICK_USERLEVEL || CLICK_TOOL
#include <syslog.h>
#include <click/error-syslog.hh>
#endif
CLICK_DECLS

/** @file error.hh
 * @brief Flexible error handling classes.
 */


#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
//
// SYSLOG ERROR HANDLER
//

 bool SyslogErrorHandler::_enabled = false;

SyslogErrorHandler::SyslogErrorHandler(int level, bool verbose, const String &context)
//: _context(context)
{
    _level = level;
    _verbose = verbose;
	int flags = LOG_CONS | LOG_NDELAY | (verbose ? LOG_PERROR : 0);
	openlog("Click", flags, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));

	_default_flags = 0;
}

void
SyslogErrorHandler::set_level(int level)
{
    _level = level;
    setlogmask(LOG_UPTO(level));
}

void SyslogErrorHandler::set_verbose(bool verbose)
{
    if (verbose != _verbose) {
        _verbose = verbose;
        int flags = LOG_CONS | LOG_NDELAY | (verbose ? LOG_PERROR : 0);
        closelog();
        openlog("Click", flags, LOG_LOCAL4);
    }
}

String
SyslogErrorHandler::vformat(const char *fmt, va_list val)
{
    return vxformat(_default_flags, fmt, val);
}

void *
SyslogErrorHandler::emit(const String &str, void *, bool)
{
	int level;

    String landmark;   
    const char *s = parse_anno(str, str.begin(), str.end(),
			       "#<>", &level, "l", &landmark, (const char *) 0);
    StringAccum sa;
    sa << _context << clean_landmark(landmark, true)
       << str.substring(s, str.end()) << '\n';
	syslog(level, "%s", sa.c_str());
    return 0;
}

void
SyslogErrorHandler::account(int level)
{
    ErrorHandler::account(level);
    if (level <= el_abort)
	abort();
    else if (level <= el_fatal)
	exit(-level);
}

int
SyslogErrorHandler::notice(const char *fmt, ...)
{
    va_list val;
    va_start(val, fmt);
    int r = xmessage(String::make_stable(e_notice, 3), fmt, val);
    va_end(val);
    return r;
}

void
SyslogErrorHandler::lnotice(const String &landmark, const char *fmt, ...)
{
    va_list val;
    va_start(val, fmt);
    String l = make_landmark_anno(landmark);
    xmessage(String::make_stable(e_notice, 3) + l, fmt, val);
    va_end(val);
}

void SyslogErrorHandler::enable()
{
	if (!SyslogErrorHandler::_enabled) {
		ErrorHandler::set_default_handler(this);
		SyslogErrorHandler::_enabled = true;
	}
}

#endif

CLICK_ENDDECLS
