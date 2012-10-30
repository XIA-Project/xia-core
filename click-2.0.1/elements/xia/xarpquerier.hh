#ifndef CLICK_XARPQUERIER_HH
#define CLICK_XARPQUERIER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/xid.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include "xarptable.hh"
#include "xcmp.hh"
CLICK_DECLS

/*
=c

XARPQuerier(XIP, ETH, I<keywords>)
XARPQuerier(NAME, I<keywords>)

=s xarp

encapsulates XIP packets in Ethernet headers found via XARP

=d

Handles most of the XARP protocol. Argument IP should be
this host's XIP address, and ETH should be this host's
Ethernet address. (In
the one-argument form, NAME should be shorthand for
both an XIP and an Ethernet address; see AddressInfo(n).)

Packets arriving on input 0 should be XIP packets, and must have their
destination address annotations set.
If an Ethernet address is already known
for the destination, the XIP packet is wrapped in an Ethernet
header and sent to output 0. Otherwise the XIP packet is saved and
an XARP query is sent instead. If an XARP response arrives
on input 1 for an XIP address that we need, the mapping is
recorded and any saved XIP packets are sent.

The XARP reply packets on input 1 should include the Ethernet header.

XARPQuerier may have one or two outputs. If it has two, then XARP queries
are sent to the second output.

XARPQuerier implements special behavior for 0.0.0.0, 255.255.255.255, multicast
addresses, and, if specified, any BROADCAST address.  Packets addressed to
0.0.0.0 are dropped.  Packets for broadcast addresses are forwarded with
destination Ethernet address FF-FF-FF-FF-FF-FF.  Multicast XIP addresses are
forwarded to 01-00-5E-xx-yy-zz, where xx-yy-zz are the lower 23 bits of the
multicast XIP address, as specified in RFC1112.

Keyword arguments are:

=over 8

=item TABLE

Element.  Names an XARPTable element that holds this element's corresponding
XARP state.  By default XARPQuerier creates its own internal XARPTable and uses
that.  If TABLE is specified, CAPACITY, ENTRY_CAPACITY, and TIMEOUT are
ignored.

=item CAPACITY

Unsigned integer.  The maximum number of saved XIP packets the table will
hold at a time.  Default is 2048.

=item ENTRY_CAPACITY

Unsigned integer.  The maximum number of XARP entries the table will hold
at a time.  Default is 0, which means unlimited.

=item TIMEOUT

Amount of time before an XARP entry expires.  Defaults to 5 minutes.

=item POLL_TIMEOUT

Amount of time after which XARPQuerier will start polling for renewal.  0 means
don't poll.  Defaults to one minute.

=item BROADCAST

XIP address.  Local broadcast XIP address.  Packets sent to this address will be
forwarded to Ethernet address FF-FF-FF-FF-FF-FF.  Defaults to the local
broadcast address that can be extracted from the XIP address's corresponding
prefix, if any.

=item BROADCAST_POLL

Boolean.  If true, then send broadcast XARP polls (where an entry is about to
expire, but hasn't expired yet).  The default is to send such polls unicast to
the known Ethernet address.  Defaults to false.

=back

=e

   c :: Classifier(12/0806 20/0002, 12/0800, ...);
   a :: XARPQuerier(18.26.4.24, 00:00:C0:AE:67:EF);
   c[0] -> a[1];
   c[1] -> ... -> a[0];
   a[0] -> ... -> ToDevice(eth0);

=n

If a host has multiple interfaces, it will need multiple
instances of XARPQuerier.

XARPQuerier uses packets' destination XIP address annotations, and can destroy
their next packet annotations.  Generated XARP queries have VLAN TCI
annotations set from the corresponding input packets.

XARPQuerier will send at most 10 queries a second for any XIP address.

=h ipaddr rw

Returns or sets the XARPQuerier's source XIP address.

=h broadcast r

Returns the XARPQuerier's XIP broadcast address.

=h table r

Returns a textual representation of the XARP table.  See XARPTable's table
handler.

=h stats r

Returns textual statistics (queries and drops).

=h queries r

Returns the number of queries sent.

=h responses r

Returns the number of responses received.

=h drops r

Returns the number of packets dropped.

=h count r

Returns the number of entries in the XARP table.

=h length r

Returns the number of packets stored in the XARP table.

=h insert w

Add an entry to the XARP table.  The input string should have the form "XIP ETH".

=h delete w

Delete an entry from the XARP table.  The input string should be an XIP address.

=h clear w

Clear the XARP table.

=a

XARPTable, XARPResponder, XARPFaker, AddressInfo
*/

class XARPQuerier : public Element { public:

    XARPQuerier();
    ~XARPQuerier();

    const char *class_name() const		{ return "XARPQuerier"; }
    const char *port_count() const		{ return "2/1-2"; }
    const char *processing() const		{ return PUSH; }
    const char *flow_code() const		{ return "xy/x"; }
    // click-undead should consider all paths live (not just "xy/x"):
    const char *flags() const			{ return "L2"; }
    void *cast(const char *name);

    int configure(Vector<String> &, ErrorHandler *);
    int live_reconfigure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    int initialize(ErrorHandler *errh);
    void add_handlers();
    void cleanup(CleanupStage stage);
    void take_state(Element *e, ErrorHandler *errh);

    void push(int port, Packet *p);

  private:

    XARPTable *_xarpt;
    EtherAddress _my_en;
    IPAddress _my_ip;
    IPAddress _my_bcast_ip;
    XID       _my_xid;
    XID       _my_bcast_xid;
    uint32_t _poll_timeout_j;
    int _broadcast_poll;

    // statistics
    atomic_uint32_t _xarp_queries;
    atomic_uint32_t _drops;
    atomic_uint32_t _xarp_responses;
    atomic_uint32_t _broadcasts;
    bool _my_xarpt;
    bool _zero_warned;
    bool _my_xid_configured;

    void send_query_for(const Packet *p, bool ether_dhost_valid);

    //void handle_ip(Packet *p, bool response);
    void handle_xip(Packet *p, bool response);
    void handle_response(Packet *p);
    void xarp_query_timeout(Packet *p);

    static void expire_hook(Timer *, void *);
    static String read_table(Element *, void *);
    static String read_table_xml(Element *, void *);
    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

    enum { h_table, h_table_xml, h_stats, h_insert, h_delete, h_clear,
	   h_count, h_length };

};

CLICK_ENDDECLS
#endif
