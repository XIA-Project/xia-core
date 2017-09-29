#ifndef CLICK_XIACIDFILITER_HH
#define CLICK_XIACIDFILITER_HH

#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/hashtable.hh>

#include <string>

CLICK_DECLS


#define PORT_IN_XCACHE 0
#define PORT_IN_NETWORK 1
#define PORT_OUT_XCACHE 0


/**
 * XIACidFilter forwards filtered CID source packets to xcache.
 */

class XIACidFilter : public Element {
private:
	void blacklist(Packet *p);
	int blacklisted(String id);
	bool enabled;
	HashTable<String, int> _blacklist;

public:
	XIACidFilter();
	~XIACidFilter();
	const char *class_name() const		{ return "XIACidFilter"; }
	const char *port_count() const		{ return "2/1"; }
	const char *processing() const		{ return PUSH; }
	int configure(Vector<String> &, ErrorHandler *);
	void push(int port, Packet *);
	void handleXcachePacket(Packet *p);
	void handleNetworkPacket(Packet *p);

	void add_handlers();
	static int toggle(const String &conf, Element *e, void *vparam, ErrorHandler *errh);
	static String status(Element *e, void *thunk);
};

CLICK_ENDDECLS
#endif
