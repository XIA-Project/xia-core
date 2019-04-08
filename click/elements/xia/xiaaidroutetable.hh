#ifndef CLICK_XIAAIDROUTETABLE_HH
#define CLICK_XIAAIDROUTETABLE_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/xiapath.hh>
#include "xcmp.hh"
CLICK_DECLS

/* NOTE: This code is a copy of xiaxidroutetable.hh and should just be
 * making a subclass instead of copying the code. Only for testing purpose.
 * -NITIN
 */

/*
=c
XIAAIDRouteTable(XID1 OUT1, XID2 OUT2, ..., - OUTn)

=s ip
simple AID routing table

=d
AID packets from network are to be delivered to corresponding IP addr locations

=e

XIAAIDRouteTable(AD0 0, HID2 1, - 2)
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

typedef struct {
	int port;
	unsigned flags;
	struct sockaddr_in* nexthop;
	//XID *nexthop;
} XIAAIDRouteData;

class XIAAIDRouteTable : public XIAXIDRouteTable { public:

    XIAAIDRouteTable();
    ~XIAAIDRouteTable();

    const char *class_name() const		{ return "XIAAIDRouteTable"; }
    const char *port_count() const		{ return "-/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    virtual void add_handlers();

    void push(int in_ether_port, Packet *);

	int set_enabled(int e);
	int get_enabled();

protected:
    int lookup_route(Packet* p);
    //int process_xcmp_redirect(Packet *);

    //static int set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    //static int set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    //static int remove_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
	//static String read_handler(Element *e, void *thunk);
	//static int write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh);

    static String list_routes_handler(Element *e, void *thunk);

private:
	String addr_str(struct sockaddr_in* addr);
	void ProcessRegistration(WritablePacket* p_in);
	void ProcessAIDPacket(WritablePacket* p_in);

	HashTable<XID, XIAAIDRouteData*> _rts;
	XIAAIDRouteData _rtdata {-7, 0, NULL};
	int _sockfd;
	/*
	HashTable<XID, XIARouteData*> _rts;
	XIARouteData _rtdata;
    uint32_t _drops;
	XIAPath _local_addr;
	XID _xcache_sid;

	int _principal_type_enabled;
	*/
};

CLICK_ENDDECLS
#endif
