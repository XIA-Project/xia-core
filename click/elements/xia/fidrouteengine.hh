#ifndef CLICK_FIDROUTEENGINE_HH
#define CLICK_FIDROUTEENGINE_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/xiapath.hh>
#include <click/xidtuple.hh>
#include "xcmp.hh"
CLICK_DECLS

/*
=c
FIDRouteEngine(XID1 OUT1, XID2 OUT2, ..., - OUTn)

=s ip
simple XID routing table

=d
Routes XID according to a routing table.

=e

FIDRouteEngine(AD0 0, HID2 1, - 2)
It outputs AD0 packets to port 0, HID2 packets to port 1, and other packets to port 2.
If the packet has already arrived at the destination node, the packet will be destroyed,
so use the XIACheckDest element before using this element.

=a StaticIPLookup, IPRouteTable
*/

#define TOTAL_SPECIAL_CASES 8
#define DESTINED_FOR_DISCARD -1
#define DESTINED_FOR_LOCALHOST -2
#define DESTINED_FOR_FLOOD -3
#define DESTINED_FOR_BROADCAST -4
#define REDIRECT -5
#define UNREACHABLE -6
#define FALLBACK -7

// special flood destination (localhost and re-flood)
#define DESTINED_FOR_FLOOD_ALL -8

struct seq_info {
	uint32_t seq;
	uint32_t tstamp;
	time_t changed;
};


class FIDRouteEngine : public Element { public:

	FIDRouteEngine();
	~FIDRouteEngine();

	const char *class_name() const		{ return "FIDRouteEngine"; }
	const char *port_count() const		{ return "-/-"; }
	const char *processing() const		{ return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);
	void add_handlers();

	void push(int in_ether_port, Packet *);

	int set_enabled(int e);
	int get_enabled();

protected:
	int lookup_route(int in_ether_port, Packet *);

	static int set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
	static int set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
	static int remove_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
	static String read_handler(Element *e, void *thunk);
	static int write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh);
	static String list_routes_handler(Element *e, void *thunk);

	void run_timer(Timer *timer);

	bool check(XIDtuple &xt, Packet *p);


private:
	HashTable<XID, XIARouteData*> _rts;
	HashTable<XIDtuple, seq_info> _seq_nos;
	XIARouteData _rtdata;
	uint32_t _drops;
	Timer _timer;

	int _principal_type_enabled;
	int _num_ports;
	XID _local_hid;
	XID _bcast_xid;
	XID _flood_xid;
};

CLICK_ENDDECLS
#endif
