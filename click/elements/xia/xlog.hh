#ifndef CLICK_XLOG_HH
#define CLICK_XLOG_HH
#include <click/element.hh>
#include <click/error-syslog.hh>
CLICK_DECLS

#ifdef PERF
#define TRACE()
#define DBG(...)
#define INFO(...)
#define NOTICE(...)
#else
#define TRACE()     _errh->debug("%s: %d", __FUNCTION__, __LINE__)
#define DBG(...)    _errh->ldebug(__FUNCTION__, __VA_ARGS__)   // level 7
#define INFO(...)   _errh->lmessage(__FUNCTION__, __VA_ARGS__) // level 6
#define NOTICE(...) _errh->lnotice(__FUNCTION__, __VA_ARGS__)  // level 5
#endif
#define WARN(...)   _errh->lwarning(__FUNCTION__, __VA_ARGS__) // level 4
#define ERROR(...)  _errh->lerror(__FUNCTION__, __VA_ARGS__)   // level 3

class XLog : public Element { public:

    XLog();
    ~XLog();

    const char *class_name() const		{ return "XLog"; }
    const char *port_count() const		{ return PORTS_0_0; }
    const char *processing() const		{ return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    int set_verbose(bool b) { _verbose = b; _errh->set_verbose(b); return 0; } ;
    int verbose() { return _verbose; } ;

    int set_level(int l) { _level = l; _errh->set_level(l); return 0; } ;
    int level() { return _level; } ;

  private:
    bool _verbose;
    int  _level;
    SyslogErrorHandler *_errh;

	enum{VERBOSE, LEVEL};
	static int write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh);
    static String read_handler(Element *e, void *thunk);
};

CLICK_ENDDECLS
#endif
