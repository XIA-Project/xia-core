// -*- related-file-name: "../../lib/syslog-handler.cc" -*-
#ifndef ERROR_SYSLOG_HH
#define ERROR_SYSLOG_HH
#include <click/error.hh>


#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
/** @class SyslogErrorHandler
 * @brief An ErrorHandler that prints error messages to a syslog.
 */
class SyslogErrorHandler : public ErrorHandler { 
public:
    /** @brief Construct a SyslogErrorHandler.
     * @param prefix string to prefix every error line */
    SyslogErrorHandler(int level, bool verbose = false, const String &prefix = String());

    void set_default_flags(int default_flags) { _default_flags = default_flags; } ;
    void set_level(int level);
    void set_verbose(bool verbose);

    String vformat(const char *fmt, va_list val);
    void *emit(const String &str, void *user_data, bool more);
    void account(int level);
    void enable();

    int notice(const char *fmt, ...);
    void lnotice(const String &landmark, const char *fmt, ...);

  private:
    static bool _enabled;
    String _context;
    int _default_flags;
    int _level;
    bool _verbose;
};
#endif

CLICK_ENDDECLS
#endif
