/*
 * xarpquerier.{cc,hh} -- XARP resolver element
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2005 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc. 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */
 
// Modified from original ARP code 

#include <click/config.h>
#include "xarpquerier.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/xid.hh>
#include <clicknet/xia.h>
#include <click/packet_anno.hh>
CLICK_DECLS

XARPQuerier::XARPQuerier()
    : _xarpt(0), _my_xarpt(false), _zero_warned(false), _my_xid_configured(false)
{
}

XARPQuerier::~XARPQuerier()
{
}

void *
XARPQuerier::cast(const char *name)
{
    if (strcmp(name, "XARPTable") == 0)
	return _xarpt;
    else if (strcmp(name, "XARPQuerier") == 0)
	return this;
    else
	return Element::cast(name);
}

int
XARPQuerier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t capacity, entry_capacity;
    Timestamp timeout, poll_timeout(60);
    bool have_capacity, have_entry_capacity, have_timeout, have_broadcast,
	broadcast_poll = false;
    _xarpt = 0;
    if (Args(this, errh).bind(conf)
	.read("CAPACITY", capacity).read_status(have_capacity)
	.read("ENTRY_CAPACITY", entry_capacity).read_status(have_entry_capacity)
	.read("TIMEOUT", timeout).read_status(have_timeout)
	.read("BROADCAST", _my_bcast_xid).read_status(have_broadcast)
	.read("TABLE", ElementCastArg("XARPTable"), _xarpt)
	.read("POLL_TIMEOUT", poll_timeout)
	.read("BROADCAST_POLL", broadcast_poll)
	.consume() < 0)
	return -1;

    if (!_xarpt) {
	Vector<String> subconf;
	if (have_capacity)
	    subconf.push_back("CAPACITY " + String(capacity));
	if (have_entry_capacity)
	    subconf.push_back("ENTRY_CAPACITY " + String(entry_capacity));
	if (have_timeout)
	    subconf.push_back("TIMEOUT " + timeout.unparse());
	_xarpt = new XARPTable;
	_xarpt->attach_router(router(), -1);
	_xarpt->configure(subconf, errh);
	_my_xarpt = true;
    }

    IPAddress my_mask;
    if (conf.size() == 1)
	conf.push_back(conf[0]);
	
    if (cp_va_kparse(conf, this, errh,
	             "XID", cpkP + cpkM, cpXID, &_my_xid,
	             "ETH", cpkP + cpkM, cpEtherAddress, &_my_en,
	             cpEnd) < 0)
	return -1;
	
    /*	
    if (Args(conf, this, errh)
	.read_mp("XID", _my_xid)
	.read_mp("ETH", _my_en)
	.complete() < 0)
	return -1;
    */

    _my_xid_configured = true;	

    if (!have_broadcast) {
    	String _bcast_xid("HID:1111111111111111111111111111111111111111");  // Broadcast HID
        _my_bcast_xid.parse(_bcast_xid);
    }
    
    _broadcast_poll = broadcast_poll;
    if ((uint32_t) poll_timeout.sec() >= (uint32_t) 0xFFFFFFFFU / CLICK_HZ)
	_poll_timeout_j = 0;
    else
	_poll_timeout_j = poll_timeout.jiffies();

    return 0;
}

int
XARPQuerier::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t capacity, entry_capacity;
    Timestamp timeout, poll_timeout(Timestamp::make_jiffies((click_jiffies_t) _poll_timeout_j));
    bool have_capacity, have_entry_capacity, have_timeout, have_broadcast,
	broadcast_poll(_broadcast_poll);
    IPAddress my_bcast_ip;

    if (Args(this, errh).bind(conf)
	.read("CAPACITY", capacity).read_status(have_capacity)
	.read("ENTRY_CAPACITY", entry_capacity).read_status(have_entry_capacity)
	.read("TIMEOUT", timeout).read_status(have_timeout)
	.read("BROADCAST", _my_bcast_xid).read_status(have_broadcast)
	.read_with("TABLE", AnyArg())
	.read("POLL_TIMEOUT", poll_timeout)
	.read("BROADCAST_POLL", broadcast_poll)
	.consume() < 0)
	return -1;

    IPAddress my_ip, my_mask;
    XID my_xid;
    EtherAddress my_en;
    if (conf.size() == 1)
	conf.push_back(conf[0]);
	
    if (cp_va_kparse(conf, this, errh,
	             "XID", cpkP + cpkM, cpXID, &_my_xid,
	             "ETH", cpkP + cpkM, cpEtherAddress, &_my_en,
	             cpEnd) < 0)
	return -1;
	
    /*	
    if (Args(conf, this, errh)
	.read_mp("XID", _my_xid)
	.read_mp("ETH", _my_en)
	.complete() < 0)
	return -1;
    */
	
    if (!have_broadcast) {
    	String _bcast_xid("HID:1111111111111111111111111111111111111111");  // Broadcast HID
        _my_bcast_xid.parse(_bcast_xid);
    }

    if ((my_xid != _my_xid || my_en != _my_en) && _my_xarpt)
	_xarpt->clear();

    _my_xid = my_xid;
    _my_en = my_en;
    String _bcast_xid("HID:1111111111111111111111111111111111111111");  // Broadcast HID
    _my_bcast_xid.parse(_bcast_xid);
    if (_my_xarpt && have_capacity)
	_xarpt->set_capacity(capacity);
    if (_my_xarpt && have_entry_capacity)
	_xarpt->set_entry_capacity(entry_capacity);
    if (_my_xarpt && have_timeout)
	_xarpt->set_timeout(timeout);

    _broadcast_poll = broadcast_poll;
    if ((uint32_t) poll_timeout.sec() >= (uint32_t) 0xFFFFFFFFU / CLICK_HZ)
	_poll_timeout_j = 0;
    else
	_poll_timeout_j = poll_timeout.jiffies();

    return 0;
}

int
XARPQuerier::initialize(ErrorHandler *)
{
    _xarp_queries = 0;
    _drops = 0;
    _xarp_responses = 0;
    return 0;
}

void
XARPQuerier::cleanup(CleanupStage stage)
{
    if (_my_xarpt) {
	_xarpt->cleanup(stage);
	delete _xarpt;
    }
}

void
XARPQuerier::take_state(Element *e, ErrorHandler *errh)
{
    XARPQuerier *xarpq = (XARPQuerier *) e->cast("XARPQuerier");
    if (!xarpq || _my_xid != xarpq->_my_xid || _my_en != xarpq->_my_en
	|| _my_bcast_xid != xarpq->_my_bcast_xid)
	return;

    if (_my_xarpt && xarpq->_my_xarpt)
	_xarpt->take_state(xarpq->_xarpt, errh);
    _xarp_queries = xarpq->_xarp_queries;
    _drops = xarpq->_drops;
    _xarp_responses = xarpq->_xarp_responses;
}

void
XARPQuerier::send_query_for(const Packet *p, bool ether_dhost_valid)
{
    // Uses p's XIP and Ethernet headers.
    static_assert(Packet::default_headroom >= sizeof(click_ether), "Packet::default_headroom must be at least 14.");
    WritablePacket *q = Packet::make(Packet::default_headroom - sizeof(click_ether),
				     NULL, sizeof(click_ether) + sizeof(click_ether_xarp), 0);
    if (!q) {
	click_chatter("in xarp querier: cannot make packet!");
	return;
    }

    click_ether *e = (click_ether *) q->data();
    q->set_ether_header(e);
    if (ether_dhost_valid && likely(!_broadcast_poll))
	memcpy(e->ether_dhost, p->ether_header()->ether_dhost, 6);
    else
	memset(e->ether_dhost, 0xff, 6);
    memcpy(e->ether_shost, _my_en.data(), 6);
    e->ether_type = htons(ETHERTYPE_XARP);

    click_ether_xarp *ea = (click_ether_xarp *) (e + 1);
    ea->ea_hdr.ar_hrd = htons(XARPHRD_ETHER);
    ea->ea_hdr.ar_pro = htons(ETHERTYPE_XIP);
    ea->ea_hdr.ar_hln = 6;
    ea->ea_hdr.ar_pln = 24;
    ea->ea_hdr.ar_op = htons(XARPOP_REQUEST);
    memcpy(ea->xarp_sha, _my_en.data(), 6);
    memcpy(ea->xarp_spa, _my_xid.data(), 24);
    memset(ea->xarp_tha, 0, 6);
        
    XID want_xid = p->nexthop_neighbor_xid_anno();
    
    memcpy(ea->xarp_tpa, want_xid.data(), 24);

    q->set_timestamp_anno(p->timestamp_anno());
    SET_VLAN_TCI_ANNO(q, VLAN_TCI_ANNO(p));

    _xarp_queries++;
    //output(noutputs() - 1).push(q);
    output(0).push(q);
}

/*
 * If the packet's XID is in the table, add an ethernet header
 * and push it out.
 * Otherwise push out a query packet.
 * May save the packet in the XARP table for later sending.
 * May call p->kill().
 */
void
XARPQuerier::handle_xip(Packet *p, bool response)
{
    // delete packet if we are not configured
    if (!_my_xid_configured) {
	p->kill();
	++_drops;
	return;
    }

    // make room for Ethernet header
    WritablePacket *q;
    if (response) {
	assert(!p->shared());
	q = p->uniqueify();
    } else if (!(q = p->push_mac_header(sizeof(click_ether)))) {
	++_drops;
	return;
    } else
	q->ether_header()->ether_type = htons(ETHERTYPE_XIP);
    
    XID nexthop_neighbor_xid = q->nexthop_neighbor_xid_anno();

    EtherAddress *dst_eth = reinterpret_cast<EtherAddress *>(q->ether_header()->ether_dhost);
    int r;

    // Easy case: requires only read lock
    retry_read_lock:
    r = _xarpt->lookup(nexthop_neighbor_xid, dst_eth, _poll_timeout_j);
    if (r >= 0) {
	assert(!dst_eth->is_broadcast());
	if (r > 0)
	    send_query_for(q, true);
	// ... and send packet below.
    } else if (nexthop_neighbor_xid == _my_bcast_xid) {
	memset(dst_eth, 0xff, 6);
    } else {
	// Zero or unknown address: do not send the packet.
	if (false) {
	    if (!_zero_warned) {
		click_chatter("%s: would query for 0; missing nexthop_neighbor_xid annotation?", declaration().c_str());
		_zero_warned = true;
	    }
	    ++_drops;
	    q->kill();
	} else {
	    r = _xarpt->append_query(nexthop_neighbor_xid, q);
	    if (r == -EAGAIN)
		goto retry_read_lock;
	    if (r > 0)
		send_query_for(q, false); // q is on the XARP entry's queue
	    // Do not q->kill() since it is stored in some XARP entry.
	}
	return;
    }

    // It's time to emit the packet with our Ethernet address as source.  (Set
    // the source address immediately before send in case the user changes the
    // source address while packets are enqueued.)
    memcpy(&q->ether_header()->ether_shost, _my_en.data(), 6);
    output(0).push(q);
}

/*
 * Got an XARP response.
 * Update our XARP table.
 * If there was a packet waiting to be sent, return it.
 */
void
XARPQuerier::handle_response(Packet *p)
{
    if (p->length() < sizeof(click_ether) + sizeof(click_ether_xarp))
	return;

    ++_xarp_responses;

    click_ether *ethh = (click_ether *) p->data();
    click_ether_xarp *xarph = (click_ether_xarp *) (ethh + 1);
    
    struct click_xia_xid xid_tmp;
    memcpy( &(xid_tmp.type), xarph->xarp_spa, 4); 
    for (size_t d = 0; d < sizeof(xid_tmp.id); d++) {
    	xid_tmp.id[d] = xarph->xarp_spa[4 + d];
    }
    XID xida = xid_tmp;
    
    EtherAddress ena = EtherAddress(xarph->xarp_sha);
    if (ntohs(ethh->ether_type) == ETHERTYPE_XARP
	&& ntohs(xarph->ea_hdr.ar_hrd) == XARPHRD_ETHER
	&& ntohs(xarph->ea_hdr.ar_pro) == ETHERTYPE_XIP
	&& ntohs(xarph->ea_hdr.ar_op) == XARPOP_REPLY
	&& !ena.is_group()) {
	Packet *cached_packet;
	_xarpt->insert(xida, ena, &cached_packet);

	// Send out packets in the order in which they arrived
	while (cached_packet) {
	    Packet *next = cached_packet->next();
	    handle_xip(cached_packet, true);
	    cached_packet = next;
	}
    }
}


void
XARPQuerier::push(int port, Packet *p)
{
    if (port == 0)
	handle_xip(p, false);
    else {
	handle_response(p);
	p->kill();
    }
}


/*
 * If xarp query timer expires, it sends 'xarp query timeout message' to XCMP module (via output port 1) 
 */
void
XARPQuerier::xarp_query_timeout(Packet *p)
{
    // Paint the packet with the XARP_timeout color	
    int anno = XIA_PAINT_ANNO_OFFSET;
    int color = UNREACHABLE;
    p->set_anno_u8(anno, color);
    output(noutputs() - 1).push(p);
}

String
XARPQuerier::read_handler(Element *e, void *thunk)
{
    XARPQuerier *q = (XARPQuerier *)e;
    switch (reinterpret_cast<uintptr_t>(thunk)) {
    case h_table:
	return q->_xarpt->read_handler(q->_xarpt, (void *) (uintptr_t) XARPTable::h_table);
    case h_stats:
	return
	    String(q->_drops.value() + q->_xarpt->drops()) + " packets killed\n" +
	    String(q->_xarp_queries.value()) + " XARP queries sent\n";
    case h_count:
	return String(q->_xarpt->count());
    case h_length:
	return String(q->_xarpt->length());
    default:
	return String();
    }
}

int
XARPQuerier::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    XARPQuerier *q = (XARPQuerier *) e;
    switch (reinterpret_cast<uintptr_t>(thunk)) {
    case h_insert:
	return q->_xarpt->write_handler(str, q->_xarpt, (void *) (uintptr_t) XARPTable::h_insert, errh);
    case h_delete:
	return q->_xarpt->write_handler(str, q->_xarpt, (void *) (uintptr_t) XARPTable::h_delete, errh);
    case h_clear:
	q->_xarp_queries = q->_drops = q->_xarp_responses = 0;
	q->_xarpt->clear();
	return 0;
    default:
	return -1;
    }
}

void
XARPQuerier::add_handlers()
{
    add_read_handler("table", read_handler, h_table);
    add_read_handler("stats", read_handler, h_stats);
    add_read_handler("count", read_handler, h_count);
    add_read_handler("length", read_handler, h_length);
    add_data_handlers("queries", Handler::OP_READ, &_xarp_queries);
    add_data_handlers("responses", Handler::OP_READ, &_xarp_responses);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_write_handler("insert", write_handler, h_insert);
    add_write_handler("delete", write_handler, h_delete);
    add_write_handler("clear", write_handler, h_clear);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XARPQuerier)
ELEMENT_REQUIRES(XARPTable)
ELEMENT_MT_SAFE(XARPQuerier)
