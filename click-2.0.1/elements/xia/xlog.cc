/*
 * xlog.{cc,hh} -- element prints packet contents to system log
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/error-syslog.hh>
#include "xlog.hh"

CLICK_DECLS

XLog::XLog()
{
#if CLICK_USERLEVEL

#endif
}

XLog::~XLog()
{
}

int
XLog::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _level = 5;
    _verbose = false;
  
    if (cp_va_kparse(conf, this, errh,
		   "LEVEL", cpkP, cpInteger, &_level,
		   "VERBOSE", 0, cpBool, &_verbose,
		   cpEnd) < 0)
		return -1;

    return 0;
}

int
XLog::initialize(ErrorHandler *)
{
#if CLICK_USERLEVEL
    // bring up logging
    _errh = new SyslogErrorHandler(_level, _verbose);
    _errh->enable();

    // FIXME: do we have to delete old handlers
    ErrorHandler::set_default_handler((ErrorHandler*)_errh);
#endif
    return 0;
}

void
XLog::cleanup(CleanupStage)
{
}

String
XLog::read_handler(Element *e, void *thunk)
{
	XLog *l = (XLog *) e;
    switch ((intptr_t)thunk) {
		case VERBOSE:
			return String(l->verbose()) + "\n";

        case LEVEL:
            return String(l->level()) + "\n";
		default:
			return "<error>";
    }
}

int 
XLog::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
	XLog *l = (XLog *) e;
    switch ((intptr_t)thunk) {
		case VERBOSE:
			return l->set_verbose(atoi(str.c_str()));
        case LEVEL:
            return l->set_level(atoi(str.c_str()));
		default:
			return -1;
    }
}

void
XLog::add_handlers()
{
	add_write_handler("verbose", write_handler, (void *)VERBOSE);
    add_read_handler("verbose", read_handler, (void *)VERBOSE);
    add_write_handler("level", write_handler, (void *)LEVEL);
	add_read_handler("level", read_handler, (void *)LEVEL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XLog)
