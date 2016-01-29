#ifndef CLICK_XIANEWXIDROUTETABLE_HH
#define CLICK_XIANEWXIDROUTETABLE_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/xiapath.hh>
#include "xcmp.hh"
#include "xiaxidroutetable.hh"
#include "scion.h"
#include "aesni.h"
#include "xlog.hh"
#include <click/error.hh>
#include <click/error-syslog.hh>
CLICK_DECLS

#define RTE_LOGTYPE_HSR RTE_LOGTYPE_USER2
//#define RTE_LOG_LEVEL RTE_LOG_INFO
#define RTE_LOG_LEVEL RTE_LOG_DEBUG
#define VERIFY_OF

#define INGRESS_IF(HOF)                                                        \
  (ntohl((HOF)->ingress_egress_if) >>                                          \
   (12 +                                                                       \
    8)) // 12bit is  egress if and 8 bit gap between uint32 and 24bit field
#define EGRESS_IF(HOF) ((ntohl((HOF)->ingress_egress_if) >> 8) & 0x000fff)

#define LOCAL_NETWORK_ADDRESS IPv4(10, 56, 0, 0)
#define GET_EDGE_ROUTER_IPADDR(IFID)                                           \
  rte_cpu_to_be_32((LOCAL_NETWORK_ADDRESS | IFID))

#define MAX_NUM_ROUTER 16
#define MAX_NUM_BEACON_SERVERS 1
#define MAX_IFID 2 << 12

/*
=c
XIANEWXIDRouteTable(XID1 OUT1, XID2 OUT2, ..., - OUTn)

=s ip
simple XID routing table

=d
Routes XID according to a routing table.

=e

XIANEWXIDRouteTable(AD0 0, HID2 1, - 2)
It outputs AD0 packets to port 0, HID2 packets to port 1, and other packets to port 2.
If the packet has already arrived at the destination node, the packet will be destroyed,
so use the XIACheckDest element before using this element.

=a StaticIPLookup, IPRouteTable
*/

typedef struct {
  uint8_t IP[4];
} IPAddr;

class XIANEWXIDRouteTable : public Element { public:

    XIANEWXIDRouteTable();
    ~XIANEWXIDRouteTable();

    const char *class_name() const		{ return "XIANEWXIDRouteTable"; }
    const char *port_count() const		{ return "-/-"; }
    const char *processing() const		{ return PUSH; }
  
    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    void push(int in_ether_port, Packet *);
int initialize(ErrorHandler *);
	int set_enabled(int e);
	int get_enabled();

protected:
    int lookup_route(int in_ether_port, Packet *);
    int process_xcmp_redirect(Packet *);

    static int set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int remove_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
	static String read_handler(Element *e, void *thunk);
	static int write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh);
  static int set_mtb_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static String list_routes_handler(Element *e, void *thunk);

    
    int scion_forward_packet(const struct click_xia* hdr);
  
private:
	HashTable<XID, XIARouteData*> _rts;
	HashTable<XID, IPAddr> _mts;
	XIARouteData _rtdata;
    uint32_t _drops;

	int _principal_type_enabled;
    int _num_ports;
    XIAPath _local_addr;
    XID _local_hid;
    XID _bcast_xid;


SyslogErrorHandler *_errh;
};

CLICK_ENDDECLS
#endif
