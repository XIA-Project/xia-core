
#ifndef CLICK_XIONBRIDGE_HH
#define CLICK_XIONBRIDGE_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/xiaheader.hh>
#include "scion/topo_parser.hh"
#include "scion/scionpathinfo.hh"
#include <queue>

#define HANDLER_RTN_FLAG_NOTREADY (1)
#define HANDLER_RTN_FLAG_FULLPATH (1<<1)

CLICK_DECLS

class XIONBridge : public Element {
  public:
    XIONBridge();
    ~XIONBridge() {};

    const char *class_name() const		{ return "XIONBridge"; }
    const char *port_count() const		{ return "-/-"; }
    const char *processing() const		{ return PUSH; }

    int initialize(ErrorHandler *errh);
    void initVariables();
    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    void push(int in_ether_port, Packet *);
    void run_timer(Timer *);

    static int request_scion_path_handler(int, String &, Element *, const Handler *, ErrorHandler *);
    void send_path_request(uint64_t);

  private:
    String topology_file;
    SCIONPathInfo *path_table;
    std::multimap<int, ServerElem> servers;
    std::multimap<int, RouterElem> routers;
    uint64_t scion_aid;
    uint64_t scion_adid;
    Timer _timer;

    std::vector< std::pair<int, Packet *> > pkt_cache;
};

CLICK_ENDDECLS

#endif
