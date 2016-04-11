#ifndef CLICK_XARPRESPONDER_HH
#define CLICK_XARPRESPONDER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/xid.hh>
CLICK_DECLS

/*
 * =c
 *
 * XARPResponder(IP/MASK [IP/MASK...] ETH, IP2/MASK2 ETH2, ...)
 *
 * =s arp
 *
 * generates responses to XARP queries
 *
 * =d
 *
 * Input should be XARP request packets, including the Ethernet header.
 * Forwards an XARP reply if we know the answer -- that is, if one of the
 * IPPREFIX arguments matches the requested IP address, then it outputs an XARP
 * reply giving the corresponding ETH address, otherwise the XARP request
 * packet is pushed out of output 1 (if it exists). Could be used for proxy
 * XARP as well as producing replies for a host's own address.
 *
 * The IP/MASK arguments are IP network addresses (IP address/netmask pairs).
 * The netmask can be specified in CIDR form (`C<18.26.7.0/24>') or dotted
 * decimal form (`C<18.26.7.0/255.255.255.0>').
 *
 * XARPResponder sets the device and VLAN TCI annotations on generated XARP
 * responses to the corresponding annotations from the queries.
 *
 * =n
 *
 * AddressInfo elements can simplify the arguments to XARPResponder. In
 * particular, if C<NAME> is shorthand for both an IP network address (or IP
 * address) C<IP> and an Ethernet address C<ETH>, then C<XARPResponder(NAME)> is
 * equivalent to C<XARPResponder(IP ETH)>. If C<NAME> is short for both an IP
 * address and an IP network address, then XARPResponder will prefer the IP
 * address. (You can say C<NAME:ipnet> to use the IP network address.)
 *
 * =h table r
 *
 * Return the XARPResponder's current table, with one IP network entry per
 * line.
 *
 * =h lookup "read with parameters"
 *
 * Takes an IP address as a parameter and returns the corresponding Ethernet
 * address, if any.
 *
 * =h add w
 *
 * Add new entries.  Takes a string in "IP/MASK [IP/MASK...] ETH" form.
 *
 * =h remove w
 *
 * Delete a single entry.  Takes a string in "IP/MASK" form; deletes any
 * entry with the same IP and MASK.
 *
 * =e
 *
 * Produce XARP replies for the local machine (18.26.4.24)
 * as well as proxy XARP for all machines on net 18.26.7
 * directing their packets to the local machine:
 *
 *   c :: Classifier(12/0806 20/0001, ...);
 *   ar :: XARPResponder(18.26.4.24 18.26.7.0/24 00-00-C0-AE-67-EF);
 *   c[0] -> ar;
 *   ar -> ToDevice(eth0);
 *
 * =a
 *
 * XARPQuerier, XARPFaker, AddressInfo */

class XARPResponder : public Element { public:

    XARPResponder();
    ~XARPResponder();

    const char *class_name() const		{ return "XARPResponder"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers();

    Packet *simple_action(Packet *);

    static Packet *make_response(const uint8_t target_eth[6],
				 const uint8_t target_xid[24],
				 const uint8_t src_eth[6],
				 const uint8_t src_xid[24],
				 const Packet *p = 0);

    const EtherAddress *lookup(XID xid) const {
	Vector<Entry>::const_iterator end = _v.end();
	for (Vector<Entry>::const_iterator it = _v.begin(); it != end; ++it)
	    if (xid == it->xida)
		return &it->ena;
	return 0;
    }

    bool lookup(XID xid, EtherAddress &result) const {
	if (const EtherAddress *x = lookup(xid)) {
	    result = *x;
	    return true;
	} else
	    return false;
    }

  private:

    struct Entry {
	XID xida;
	EtherAddress ena;
    };
    Vector<Entry> _v;
    EtherAddress _my_en;
    XID       _my_xid;    

    static int entry_compare(const void *a, const void *b, void *user_data);
    int add(Vector<Entry> &v, const String &arg, ErrorHandler *errh) const;
    static void normalize(Vector<Entry> &v, bool warn, ErrorHandler *errh);

    static String read_handler(Element *, void *);
    static int lookup_handler(int op, String &str, Element *e, const Handler *h, ErrorHandler *errh);
    static int add_handler(const String &s, Element *e, void *, ErrorHandler *errh);
    static int remove_handler(const String &s, Element *e, void *, ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif
