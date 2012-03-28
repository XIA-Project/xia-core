/*
 * xarpresponder.{cc,hh} -- element that responds to XARP queries
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
#include "xarpresponder.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/xid.hh>
#include <clicknet/xia.h>
CLICK_DECLS

XARPResponder::XARPResponder()
{
}

XARPResponder::~XARPResponder()
{
}

int
XARPResponder::add(Vector<Entry> &v, const String &arg, ErrorHandler *errh) const
{
    Vector<String> words;
    cp_spacevec(arg, words);

    Vector<Entry> entries;
    EtherAddress ena;
    bool have_ena = false;
    
    if (cp_va_kparse(words, this, errh,
	             "XID", cpkP + cpkM, cpXID, &_my_xid,
	             "ETH", cpkP + cpkM, cpEtherAddress, &_my_en,
	             cpEnd) < 0)
	return -1;    
    
    v.push_back(Entry());
    v.back().xida = _my_xid;    
    
    if (EtherAddressArg().parse(words[1], ena, this)) {
    	have_ena = true;
    } else {
    	return errh->error("Ethernet address format error");
    }
    
    v.back().ena = ena; 
    return 0;  
}

int
XARPResponder::entry_compare(const void *ap, const void *bp, void *user_data)
{
    int a = *reinterpret_cast<const int *>(ap),
	b = *reinterpret_cast<const int *>(bp);
    const Entry *entries = reinterpret_cast<Entry *>(user_data);
    const Entry &ea = entries[a], &eb = entries[b];

    if (ea.xida == eb.xida)
	return b - a;		// keep later match
    else
	return a - b;
}

void
XARPResponder::normalize(Vector<Entry> &v, bool warn, ErrorHandler *errh)
{
    Vector<int> permute;
    for (int i = 0; i < v.size(); ++i)
	permute.push_back(i);
    click_qsort(permute.begin(), permute.size(), sizeof(int), entry_compare, v.begin());

    Vector<Entry> nv;
    for (int i = 0; i < permute.size(); ++i) {
	const Entry &e = v[permute[i]];
	if (nv.empty() || nv.back().xida != e.xida)
	    nv.push_back(e);
	else if (warn)
	    errh->warning("multiple entries for %s", e.xida.unparse().c_str());
    }
    nv.swap(v);
}

int
XARPResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Vector<Entry> v;
    for (int i = 0; i < conf.size(); i++) {
	PrefixErrorHandler perrh(errh, "argument " + String(i) + ": ");
	add(v, conf[i], &perrh);
    }
    if (!errh->nerrors()) {
	normalize(v, true, errh);
	_v.swap(v);
	return 0;
    } else
	return -1;
}

Packet *
XARPResponder::make_response(const uint8_t target_eth[6], /* them */
                            const uint8_t target_xid[24],
                            const uint8_t src_eth[6], /* me */
                            const uint8_t src_xid[24],
			    const Packet *p /* only used for annotations */)
{
    WritablePacket *q = Packet::make(sizeof(click_ether) + sizeof(click_ether_xarp));
    if (q == 0) {
	click_chatter("in xarp responder: cannot make packet!");
	return 0;
    }

    // in case of FromLinux, set the device annotation: want to make it seem
    // that XARP response came from the device that the query arrived on
    if (p) {
	q->set_device_anno(p->device_anno());
	SET_VLAN_TCI_ANNO(q, VLAN_TCI_ANNO(p));
    }

    click_ether *e = (click_ether *) q->data();
    q->set_ether_header(e);
    memcpy(e->ether_dhost, target_eth, 6);
    memcpy(e->ether_shost, src_eth, 6);
    e->ether_type = htons(ETHERTYPE_XARP);

    click_ether_xarp *ea = (click_ether_xarp *) (e + 1);
    ea->ea_hdr.ar_hrd = htons(XARPHRD_ETHER);
    ea->ea_hdr.ar_pro = htons(ETHERTYPE_XIP);
    ea->ea_hdr.ar_hln = 6;
    ea->ea_hdr.ar_pln = 24;
    ea->ea_hdr.ar_op = htons(XARPOP_REPLY);
    memcpy(ea->xarp_sha, src_eth, 6);
    memcpy(ea->xarp_spa, src_xid, 24);
    memcpy(ea->xarp_tha, target_eth, 6);
    memcpy(ea->xarp_tpa, target_xid, 24);

    return q;
}

Packet *
XARPResponder::simple_action(Packet *p)
{
    const click_ether *e = (const click_ether *) p->data();
    const click_ether_xarp *ea = (const click_ether_xarp *) (e + 1);
    Packet *q = 0;
    if (p->length() >= sizeof(*e) + sizeof(click_ether_xarp)
	&& e->ether_type == htons(ETHERTYPE_XARP)
	&& ea->ea_hdr.ar_hrd == htons(XARPHRD_ETHER)
	&& ea->ea_hdr.ar_pro == htons(ETHERTYPE_XIP)
	&& ea->ea_hdr.ar_op == htons(XARPOP_REQUEST)) {
		
	struct click_xia_xid xid_tmp;
    	memcpy( &(xid_tmp.type), ea->xarp_tpa, 4); 
    	for (size_t d = 0; d < sizeof(xid_tmp.id); d++) {
    		xid_tmp.id[d] = ea->xarp_tpa[4 + d];
    	}
    	XID xid = xid_tmp;
	
	
	if (const EtherAddress *ena = lookup(xid))
	    q = make_response(ea->xarp_sha, ea->xarp_spa, ena->data(), ea->xarp_tpa, p);
    }
    if (q)
	p->kill();
    else
	checked_output_push(1, p);
    return q;
}

String
XARPResponder::read_handler(Element *e, void *)
{
    XARPResponder *ar = static_cast<XARPResponder *>(e);
    StringAccum sa;
    for (int i = 0; i < ar->_v.size(); i++)
	sa << ar->_v[i].xida.unparse() << ' ' << ar->_v[i].ena << '\n';
    return sa.take_string();
}

int
XARPResponder::lookup_handler(int, String &str, Element *e, const Handler *, ErrorHandler *errh)
{
    XARPResponder *ar = static_cast<XARPResponder *>(e);
    XID xid(str);
	if (const EtherAddress *ena = ar->lookup(xid))
	    str = ena->unparse();
	else
	    str = String();
	return 0;
}

int
XARPResponder::add_handler(const String &s, Element *e, void *, ErrorHandler *errh)
{
    XARPResponder *ar = static_cast<XARPResponder *>(e);
    Vector<Entry> v(ar->_v);
    if (ar->add(v, s, errh) >= 0) {
	normalize(v, false, 0);
	ar->_v.swap(v);
	return 0;
    } else
	return -1;
}

int
XARPResponder::remove_handler(const String &s, Element *e, void *, ErrorHandler *errh)
{
    XARPResponder *ar = static_cast<XARPResponder *>(e);
    XID xid(s);
    for (Vector<Entry>::iterator it = ar->_v.begin(); it != ar->_v.end(); ++it)
	if (it->xida == xid) {
	    ar->_v.erase(it);
	    return 0;
	}
    return errh->error("%s not found", xid.unparse().c_str());
}


void
XARPResponder::add_handlers()
{
    add_read_handler("table", read_handler, 0);
    set_handler("lookup", Handler::OP_READ | Handler::READ_PARAM, lookup_handler);
    add_write_handler("add", add_handler, 0);
    add_write_handler("remove", remove_handler, 0);
}

EXPORT_ELEMENT(XARPResponder)
CLICK_ENDDECLS
