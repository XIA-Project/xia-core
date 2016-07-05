#ifndef CLICK_XLOG_HH
#define CLICK_XLOG_HH
#include <click/element.hh>
#include <click/error-syslog.hh>
#include <click/error.hh>

CLICK_DECLS

#ifdef DEBUG
#define TRACE()     (ErrorHandler::default_handler())->debug\
    ("%s: %d", __FUNCTION__, __LINE__)
#define DBG(...)    (ErrorHandler::default_handler())->ldebug\
    (__FUNCTION__, __VA_ARGS__)   // level 7
#define INFO(...)   (ErrorHandler::default_handler())->lmessage\
    (__FUNCTION__, __VA_ARGS__) // level 6
#define NOTICE(...) (ErrorHandler::default_handler())->xmessage\
    (String::make_stable(ErrorHandler::e_notice, 3)+__FUNCTION__+": ", \
    ErrorHandler::xformat(__VA_ARGS__))  // level 5
#define WARN(...)   (ErrorHandler::default_handler())->lwarning\
    (__FUNCTION__, __VA_ARGS__) // level 4
#define ERROR(...)  (ErrorHandler::default_handler())->lerror\
    (__FUNCTION__, __VA_ARGS__)   // level 3
//#define T_ALERT(...)  (ErrorHandler::default_handler())->fatal\ 
//    (__FUNCTION__, __VA_ARGS__)  // level -1
#else
	#define TRACE()
	#define DBG(...)
	#define INFO(...)
	#define NOTICE(...)
	#define WARN(...)
	#define ERROR(...)
#endif

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
