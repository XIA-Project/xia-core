#ifndef CLICK_ETHERVLANANNOENCAP_HH
#define CLICK_ETHERVLANANNOENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

EtherVLANAnnoEncap(ETHERTYPE, SRC, DST [, NATIVE_VLAN_ID])

=s ethernet

encapsulates packets in 802.1Q VLAN Ethernet header

=d

Encapsulates each packet in the 802.1Q header specified by its arguments.  The
resulting packet looks like an Ethernet packet with type 0x8100.  The
encapsulated Ethernet type is ETHERTYPE, which should be in host order.  The
VLAN_ID and VLAN_PCP arguments define the VLAN ID and priority code point.
VLAN_ID must be between 0 and 0xFFE.  VLAN_PCP defaults to 0, and must be
between 0 and 7.

=e

Encapsulate packets in an 802.1Q Ethernet VLAN header with type ETHERTYPE_IP
(0x0800), source address 1:1:1:1:1:1, destination address 2:2:2:2:2:2, and
VLAN ID 1:

  EtherVLANEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2, 1)

=h src read/write

Return or set the SRC parameter.

=h dst read/write

Return or set the DST parameter.

=h ethertype read/write

Return or set the ETHERTYPE parameter.

=h vlan_id read/write

Return or set the VLAN_ID parameter.

=h vlan_pcp read/write

Return or set the VLAN_PCP parameter.

=a

EtherEncap, ARPQuerier, EnsureEther, StoreEtherAddress */

class EtherVLANAnnoEncap : public Element { public:

    EtherVLANAnnoEncap();
    ~EtherVLANAnnoEncap();

    const char *class_name() const	{ return "EtherVLANEncap"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers();

    Packet *smaction(Packet *p);
    void push(int port, Packet *p);
    Packet *pull(int port);

  private:

    click_ether_vlan _ethh;
    bool _use_anno;
    bool _use_native_vlan;
    uint16_t _native_vlan;

    enum { h_vlan_id, h_vlan_pcp };
    static String read_handler(Element *e, void *user_data);
    static int write_handler(const String &s, Element *e, void *user_data, ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif
