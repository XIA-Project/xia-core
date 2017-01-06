/*
 * fidrouteengine.{cc,hh} -- route & flood FID packets
 */

#include <click/config.h>
#include "fidrouteengine.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiaheader.hh>
#include "click/xiafidheader.hh"
#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#endif
CLICK_DECLS

/*
** TODO:
**	- remove broadcast semantics
**		-- delete broadcast FID
**		-- rework lookup route to not care about broadcast
**	- check to see if we've seen the packet already
**		- discard if we have
**	- look at the FID (in our routing table)
**	- if it's ours
**		- send it up the stack
**	- else if it has an interface in the table
**		- unicast it????
**	- else
**		- send it out all the interfaces except the ingress interfaces
**
** Phase 2
**	- ignore external interfaces when reflooding
*** - tcp??
*/

FIDRouteEngine::FIDRouteEngine(): _drops(0)
{
}

FIDRouteEngine::~FIDRouteEngine()
{
	_rts.clear();
}

int
FIDRouteEngine::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_principal_type_enabled = 1;
	_num_ports = 0;

    _rtdata.port = -1;
    _rtdata.flags = 0;
    _rtdata.nexthop = NULL;

    if (cp_va_kparse(conf, this, errh,
		"NUM_PORT", cpkP+cpkM, cpInteger, &_num_ports,
		cpEnd) < 0)
	return -1;

    String broadcast_xid(BFID);  // broadcast FID
    _bcast_xid.parse(broadcast_xid);

	return 0;
}

int
FIDRouteEngine::set_enabled(int e)
{
	_principal_type_enabled = e;
	return 0;
}

int FIDRouteEngine::get_enabled()
{
	return _principal_type_enabled;
}

void
FIDRouteEngine::add_handlers()
{
	add_write_handler("add", set_handler, 0);
	add_write_handler("set", set_handler, (void*)1);
	add_write_handler("add4", set_handler4, 0);
	add_write_handler("set4", set_handler4, (void*)1);
	add_write_handler("remove", remove_handler, 0);
	add_data_handlers("drops", Handler::OP_READ, &_drops);
	add_read_handler("list", list_routes_handler, 0);
	add_write_handler("enabled", write_handler, (void *)PRINCIPAL_TYPE_ENABLED);
	add_write_handler("hid", write_handler, (void *)ROUTE_TABLE_HID);
	add_read_handler("enabled", read_handler, (void *)PRINCIPAL_TYPE_ENABLED);
}

String
FIDRouteEngine::read_handler(Element *e, void *thunk)
{
	FIDRouteEngine *t = (FIDRouteEngine *) e;
    switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return String(t->get_enabled());

		default:
			return "<error>";
    }
}

int
FIDRouteEngine::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
	FIDRouteEngine *t = (FIDRouteEngine *) e;
    switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return t->set_enabled(atoi(str.c_str()));
		case ROUTE_TABLE_HID:
		{
			XID hid;
			if (cp_va_kparse(str, t, errh,
						"HID", cpkP + cpkM, cpXID, &hid, cpEnd) < 0)
				return -1;
			t->_local_hid = hid;
			click_chatter("FIDRouteEngine: HID assigned: %s", t->_local_hid.unparse().c_str());
			return 0;
		}
		default:
			return -1;
    }
}

String
FIDRouteEngine::list_routes_handler(Element *e, void * /*thunk */)
{
	FIDRouteEngine* table = static_cast<FIDRouteEngine*>(e);
	XIARouteData *xrd = &table->_rtdata;

	// get the default route
	String tbl = "-," + String(xrd->port) + "," +
		(xrd->nexthop != NULL ? xrd->nexthop->unparse() : "") + "," +
		String(xrd->flags) + "\n";

	// get the rest
	HashTable<XID, XIARouteData *>::iterator it = table->_rts.begin();
	while (it != table->_rts.end()) {
		String xid = it.key().unparse();

		xrd = (XIARouteData *)it.value();

		tbl += xid + ",";
		tbl += String(xrd->port) + ",";
		tbl += (xrd->nexthop != NULL ? xrd->nexthop->unparse() : "") + ",";
		tbl += String(xrd->flags) + "\n";
		it++;
	}
	return tbl;
}

int
FIDRouteEngine::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	String str_copy = conf;
	String xid_str = cp_shift_spacevec(str_copy);

	if (xid_str.length() == 0) {
		return 0;
	}

	int port;
	if (!cp_integer(str_copy, &port))
		return errh->error("invalid port: ", str_copy.c_str());

	String str = xid_str + "," + String(port) + ",,0";

	return set_handler4(str, e, thunk, errh);
}

int
FIDRouteEngine::set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	FIDRouteEngine* table = static_cast<FIDRouteEngine*>(e);

	bool add_mode = !thunk;

	Vector<String> args;
	int port = 0;
	unsigned flags = 0;
	String xid_str;
	XID *nexthop = NULL;

	cp_argvec(conf, args);

	if (args.size() < 2 || args.size() > 4)
		return errh->error("invalid route: ", conf.c_str());

	xid_str = args[0];

	if (!cp_integer(args[1], &port))
		return errh->error("invalid port: ", conf.c_str());

	if (args.size() == 4) {
		if (!cp_integer(args[3], &flags))
			return errh->error("invalid flags: ", conf.c_str());
	}

	if (args.size() >= 3 && args[2].length() > 0) {
	    String nxthop = args[2];
		nexthop = new XID;
		cp_xid(nxthop, nexthop, e);
		//nexthop = new XID(args[2]);
		if (!nexthop->valid()) {
			delete nexthop;
			return errh->error("invalid next hop xid: ", conf.c_str());
		}
	}

	if (xid_str == "-") {
		if (add_mode && table->_rtdata.port != -1)
			return errh->error("duplicate default route: ", xid_str.c_str());
		table->_rtdata.port= port;
		table->_rtdata.flags = flags;
		table->_rtdata.nexthop = nexthop;
	} else {
		 XID xid;
		if (!cp_xid(xid_str, &xid, e)) {
			if (nexthop) delete nexthop;
			return errh->error("invalid XID: ", xid_str.c_str());
		}
		if (add_mode && table->_rts.find(xid) != table->_rts.end()) {
			if (nexthop) delete nexthop;
			return errh->error("duplicate XID: ", xid_str.c_str());
		}

		XIARouteData *xrd = new XIARouteData();

		xrd->port = port;
		xrd->flags = flags;
		xrd->nexthop = nexthop;
		table->_rts[xid] = xrd;
	}

	return 0;
}

int
FIDRouteEngine::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
	FIDRouteEngine* table = static_cast<FIDRouteEngine*>(e);

	if (xid_str.length() == 0) {
		return 0;
	}

	if (xid_str == "-") {
		table->_rtdata.port = -1;
		table->_rtdata.flags = 0;
		if (table->_rtdata.nexthop) {
			delete table->_rtdata.nexthop;
			table->_rtdata.nexthop = NULL;
		}

	} else {
		XID xid;
		if (!cp_xid(xid_str, &xid, e))
			return errh->error("invalid XID: ", xid_str.c_str());
		HashTable<XID, XIARouteData*>::iterator it = table->_rts.find(xid);
		if (it == table->_rts.end())
			return errh->error("nonexistent XID: ", xid_str.c_str());

		XIARouteData *xrd = (XIARouteData*)it.value();
		if (xrd->nexthop) {
			delete xrd->nexthop;
		}

		table->_rts.erase(it);
		delete xrd;
	}
	return 0;
}

void
FIDRouteEngine::push(int in_ether_port, Packet *p)
{
	int port;

	in_ether_port = XIA_PAINT_ANNO(p);

	if (!_principal_type_enabled) {
		output(2).push(p);
		return;
	}

	port = lookup_route(in_ether_port, p);

	if (port >= 0) {
		SET_XIA_PAINT_ANNO(p,port);
		output(0).push(p);

	} else if (port == DESTINED_FOR_LOCALHOST) {
		output(1).push(p);

	} else if (port == DESTINED_FOR_FLOOD_ALL) {

		// reflood the packet
		// for (int i = 0; i <= _num_ports; i++) {
		// 	if (i != in_ether_port) {
		// 		printf("reflooding\n");
		// 		Packet *q = p->clone();
		// 		SET_XIA_PAINT_ANNO(q, i);
		// 		output(0).push(q);
		// 	}
		// }`

		// and handle it locally
		output(1).push(p);

	} else if (port == DESTINED_FOR_BROADCAST) {
		// send it out on all interfaces except the one it came in on
		// FIXME: eventually restrict external interfaces as well
		for (int i = 0; i <= _num_ports; i++) {
			if (i != in_ether_port) {
				Packet *q = p->clone();
				SET_XIA_PAINT_ANNO(q, i);
				output(0).push(q);
			}
		}
		p->kill();

	} else {
		// no route, consider fallbacks
		output(2).push(p);
	}
}


int
FIDRouteEngine::lookup_route(int in_ether_port, Packet *p)
{
	const struct click_xia* hdr = p->xia_header();

	// FIXME: what edge checking still needs to happen?
	int last = hdr->last;

	if (last < 0) {
		// the last pointer was still in the initial position
		//  so the intent node holds the first edge
		last += hdr->dnode;
	}

	const struct click_xia_xid_edge* edge = hdr->node[last].edge;
	const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
	const int& idx = current_edge.idx;

	if (idx == CLICK_XIA_XID_EDGE_UNUSED) {
		// unused edge -- use default route
		return _rtdata.port;
	}

    const struct click_xia_xid_node& node = hdr->node[idx];
	XID f(node.xid);

	// we're initiating a flood
	if (in_ether_port == DESTINED_FOR_LOCALHOST) {
		// Mark packet for flooding
		//  which will duplicate the packet and send to every interface
		p->set_nexthop_neighbor_xid_anno(_bcast_xid);
		return DESTINED_FOR_BROADCAST;
	}

	// have we seen this packet already?
	// FIXME: add check to see if we have seen this packet before
	if (0) {
		// we've seen it
		return DESTINED_FOR_DISCARD;
	}

	// it's a new packet that may or may not be destined for me

	// FIXME: if we keep the global value, should we handle it like
	// this or use the routing table like below?
	if (f == _bcast_xid) {
		// it's the global FID
		// FIXME: is this a temporary case?

		// we want to handle this locally and also reflood it
		return DESTINED_FOR_FLOOD_ALL;
	}

	HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(node.xid);
	if (it != _rts.end()) {
		XIARouteData *xrd = (*it).second;

		if(xrd->port != DESTINED_FOR_LOCALHOST && xrd->port != FALLBACK && xrd->nexthop != NULL) {
			// set next hop annotation if the packet is going back out
			// this is unlikely to ever happen unless we have added specific routes for an FID
			// on a different host
			p->set_nexthop_neighbor_xid_anno(*(xrd->nexthop));
		}

		return xrd->port;

	} else {
		// not a global broadcast and not for us, just reflood it
		p->set_nexthop_neighbor_xid_anno(_bcast_xid);
		return DESTINED_FOR_BROADCAST;
	}

	assert(0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FIDRouteEngine)
ELEMENT_MT_SAFE(FIDRouteEngine)
