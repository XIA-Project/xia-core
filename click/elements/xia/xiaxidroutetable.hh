#ifndef CLICK_XIAXIDROUTETABLE_HH
#define CLICK_XIAXIDROUTETABLE_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/xiapath.hh>
#include "xcmp.hh"

#include <memory>
CLICK_DECLS

/*
=c
XIAXIDRouteTable(XID1 OUT1, XID2 OUT2, ..., - OUTn)

=s ip
simple XID routing table

=d
Routes XID according to a routing table.

=e

XIAXIDRouteTable(AD0 0, HID2 1, - 2)
It outputs AD0 packets to port 0, HID2 packets to port 1, and other packets to port 2.
If the packet has already arrived at the destination node, the packet will be destroyed,
so use the XIACheckDest element before using this element.

=a StaticIPLookup, IPRouteTable
*/

#define TOTAL_SPECIAL_CASES 8
#define DESTINED_FOR_DISCARD -1
#define DESTINED_FOR_LOCALHOST -2
#define DESTINED_FOR_DHCP -3
#define DESTINED_FOR_BROADCAST -4
#define REDIRECT -5
#define UNREACHABLE -6
#define FALLBACK -7
#define NEIGHBOR 263
//7

#define XIA_UDP_NEXTHOP 5

#define MAX_NEIGHBOR_CNT 1024 //todo: arbitrary value at this point

enum { PRINCIPAL_TYPE_ENABLED, ROUTE_TABLE_HID, FWD_TABLE_DAG, XCACHE_SID };

typedef struct {
    int port;
    unsigned flags;
    XID *nexthop;
    std::unique_ptr<struct sockaddr_in> nexthop_in;
    bool neighbor;
} XIARouteData;


typedef struct {
    std::string *AD;
    std::string *HID;
    std::string *SID;
} XIAXIDAddr;

typedef struct {
    String *addr; // ip:port
    int iface; // outgoing iface to the neighbor
    String *AD;
} XIAXIDNeighbor;

class XIAXIDRouteTable : public Element { public:

    XIAXIDRouteTable();
    ~XIAXIDRouteTable();

    const char *class_name() const      { return "XIAXIDRouteTable"; }
    const char *port_count() const      { return "-/-"; }
    const char *processing() const      { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    void push(int in_ether_port, Packet *);

    int set_enabled(int e);
    int get_enabled();
    void printRoutingTable();

protected:
    int lookup_route(Packet *);
    //int process_xcmp_redirect(Packet *);

    static int set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int add_ip_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int remove_ip_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int set_udpnext(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int remove_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static String read_handler(Element *e, void *thunk);
    static int write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh);

    static void add_entry_to_tbl_str(Element *e, String& tbl, String xid, XIARouteData* xrd);
    static String list_routes_handler(Element *e, void *thunk);
    static String list_neighbor_handler(Element *e, void *);
    static String list_ip_handler(Element *e, void *);

    HashTable<XID, XIARouteData*> _rts;
    HashTable<String, XIAXIDAddr*> _nts;
    XIARouteData _rtdata;
    uint32_t _drops;
    String _hostname;
    std::vector<XIAXIDNeighbor *> _ntable;

    int _principal_type_enabled;
};

CLICK_ENDDECLS
#endif
