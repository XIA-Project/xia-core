#ifndef CLICK_XIACIDFILITER_HH
#define CLICK_XIACIDFILITER_HH

#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
CLICK_DECLS


#define PORT_IN_XCACHE 0
#define PORT_IN_XTRANSPORT 1
#define PORT_OUT_XCACHE 0


/**
 * XIACidFilter forwards filtered CID source packets to xcache.
 */

class XIACidFilter : public Element {
public:
	XIAPath local_addr;
	XIACidFilter();
	~XIACidFilter();
	const char *class_name() const		{ return "XIACidFilter"; }
	const char *port_count() const		{ return "2/1"; }
	const char *processing() const		{ return PUSH; }
	int configure(Vector<String> &, ErrorHandler *);
	void push(int port, Packet *);
	void handleXcachePacket(Packet *p);
	void handleXtransportPacket(Packet *p);
private:
};

CLICK_ENDDECLS
#endif

