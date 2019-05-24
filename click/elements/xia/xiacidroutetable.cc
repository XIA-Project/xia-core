/*
 * xiacidroutetable.{cc,hh} -- simple XID routing table
 */

#include <click/config.h>
#include "xiacidroutetable.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiaheader.hh>
#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
CLICK_DECLS

XIACIDRouteTable::XIACIDRouteTable(): _drops(0)
{
}

XIACIDRouteTable::~XIACIDRouteTable()
{
	_rts.clear();
}

int
XIACIDRouteTable::configure(Vector<String> & /*conf*/, ErrorHandler * /*errh*/)
{
    //click_chatter("XIACIDRouteTable: configuring %s\n", this->name().c_str());

	_principal_type_enabled = 1;

    _rtdata.port = -1;
    _rtdata.flags = 0;
    _rtdata.nexthop = NULL;

	return 0;
}

int
XIACIDRouteTable::set_enabled(int e)
{
	_principal_type_enabled = e;
	return 0;
}

int XIACIDRouteTable::get_enabled()
{
	return _principal_type_enabled;
}

void
XIACIDRouteTable::add_handlers()
{
	add_write_handler("add", set_handler, 0);
	add_write_handler("set", set_handler, (void*)1);
	add_write_handler("add4", set_handler4, 0);
	add_write_handler("set4", set_handler4, (void*)1);
	add_write_handler("remove", remove_handler, 0);
	add_write_handler("load", load_routes_handler, 0);
	add_write_handler("generate", generate_routes_handler, 0);
	add_data_handlers("drops", Handler::OP_READ, &_drops);
	add_read_handler("list", list_routes_handler, 0);
	add_write_handler("enabled", write_handler, (void *)PRINCIPAL_TYPE_ENABLED);
	add_read_handler("enabled", read_handler, (void *)PRINCIPAL_TYPE_ENABLED);
	add_write_handler("dag", write_handler, (void *)FWD_TABLE_DAG);
	add_write_handler("xcache", write_handler, (void *)XCACHE_SID);
}

String
XIACIDRouteTable::read_handler(Element *e, void *thunk)
{
	XIACIDRouteTable *t = (XIACIDRouteTable *) e;
    switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return String(t->get_enabled());

		default:
			return "<error>";
    }
}

int
XIACIDRouteTable::write_handler(const String &str, Element *e, void *thunk, ErrorHandler * errh)
{
	XIACIDRouteTable *t = (XIACIDRouteTable *) e;
    switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return t->set_enabled(atoi(str.c_str()));
		case FWD_TABLE_DAG:
		{
			XIAPath dag;
			if(cp_va_kparse(str, t, errh,
						"ADDR", cpkP + cpkM, cpXIAPath, &dag,
						cpEnd) < 0) {
				return -1;
			}
			t->_local_addr = dag;
			click_chatter("XIACIDRouteTable: DAG is now %s",
					t->_local_addr.unparse().c_str());
			break;
		}
		case XCACHE_SID:
		{
			XID xcache_sid;
			if(cp_va_kparse(str, t, errh,
						"XCACHE_ID", cpkP + cpkM, cpXID, &xcache_sid,
						cpEnd) < 0) {
				return -1;
			}
			t->_xcache_sid = xcache_sid;
			click_chatter("XIACIDRouteTable: Xcache is %s",
					t->_xcache_sid.unparse().c_str());
			break;
		}
		default:
			return -1;
    }
	return 0;
}

void
XIACIDRouteTable::add_entry_to_tbl_str(String& tbl, String xid,
		XIARouteData* xrd)
{
	// XID
	tbl += xid + ",";
	// port
	tbl += String(xrd->port) + ",";
	// nexthop
	if(xrd->nexthop != NULL) {
		tbl += xrd->nexthop->unparse() + ",";
	} else if(xrd->nexthop_in != nullptr) {
		tbl += String(inet_ntoa(xrd->nexthop_in->sin_addr)) + ":";
		tbl += String(ntohs(xrd->nexthop_in->sin_port)) + ",";
	} else {
		tbl += String("") + ",";
	}
	// flags
	tbl += String(xrd->flags) + "\n";
}

String
XIACIDRouteTable::list_routes_handler(Element *e, void * /*thunk */)
{
	XIACIDRouteTable* table = static_cast<XIACIDRouteTable*>(e);
	XIARouteData *xrd = &table->_rtdata;

	// get the default route
	String tbl;
	add_entry_to_tbl_str(tbl, "-", xrd);

	// get the rest
	for(auto& it : table->_rts) {
		String xid = it.first.unparse();
		xrd = it.second;
		add_entry_to_tbl_str(tbl, xid, xrd);
	}
	return tbl;
}

int
XIACIDRouteTable::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	// handle older style route entries

	String str_copy = conf;
	String xid_str = cp_shift_spacevec(str_copy);

	if (xid_str.length() == 0)
	{
		// ignore empty entry
		return 0;
	}

	int port;
	if (!cp_integer(str_copy, &port))
		return errh->error("invalid port: ", str_copy.c_str());

	String str = xid_str + "," + String(port) + ",,0";

	return set_handler4(str, e, thunk, errh);
}

int
XIACIDRouteTable::set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	XIACIDRouteTable* table = static_cast<XIACIDRouteTable*>(e);

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
		// If nexthop is less than 40 chars, it is an IP:port
		if(nxthop.length() < CLICK_XIA_XID_ID_LEN*2) {
			return set_udpnext(conf, e, thunk, errh);
		}
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
		xrd->nexthop_in = nullptr;
		table->_rts[xid] = xrd;
	}

	return 0;
}

int
XIACIDRouteTable::set_udpnext(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	XIACIDRouteTable* table = static_cast<XIACIDRouteTable*>(e);
	bool add_mode = !thunk;
	Vector<String> args;
	int port = 0;
	unsigned flags = XIA_UDP_NEXTHOP;
	String xid_str;
	cp_argvec(conf, args);
	if(args.size() < 3) {
		// We need xid, port, nexthop_in entries
		return errh->error("Invalid route(need 3 entries): ", conf.c_str());
	}

	// First argument is the XID
	xid_str = args[0];

	// Second argument is the interface that matching packet should go out on
	if(!cp_integer(args[1], &port)) {
		return errh->error("Invalid port: ", conf.c_str());
	}

	// Third argument should be IPaddr:port of next hop
	if(args[2].length() == 0) {
		return errh->error("Invalid ipaddr: ", conf.c_str());
	}
	String nxthop = args[2];
	int separator_offset = nxthop.find_left(':', 0);
	if(separator_offset == -1) {
		return errh->error("Invalid nexthop(need ip:port): ", conf.c_str());
	}
	String ipaddrstr = nxthop.substring(0, separator_offset);
	String portstr = nxthop.substring(separator_offset+1);
	printf("XIACIDRouteTable: ip: %s, port: %s\n",
			ipaddrstr.c_str(), portstr.c_str());
	struct in_addr ipaddr;
	if(inet_aton(ipaddrstr.c_str(), &ipaddr) == 0) {
		return errh->error("Invalid ipaddr: ", ipaddrstr.c_str());
	}
	int ipport = atoi(portstr.c_str());
	if(ipport < 0 || ipport > 65535) {
		return errh->error("Invalid port: ", portstr.c_str());
	}

	// Convert address to a sockaddr_in and save into forwarding table
	auto addr = std::make_unique<struct sockaddr_in>();
	addr->sin_family = AF_INET;
	addr->sin_port = htons(ipport);
	addr->sin_addr.s_addr = ipaddr.s_addr;
	if(xid_str == "-") {
		// Save this address as default route if XID was '-'
		table->_rtdata.port = port;
		table->_rtdata.flags = flags;
		if(table->_rtdata.nexthop) {
			delete table->_rtdata.nexthop;
		}
		table->_rtdata.nexthop = NULL;
		table->_rtdata.nexthop_in = std::move(addr);
	} else {
		// Otherwise, save it in forwarding table for given XID
		XID xid;
		if (!cp_xid(xid_str, &xid, e)) {
			return errh->error("invalid XID: ", xid_str.c_str());
		}
		if (add_mode && table->_rts.find(xid) != table->_rts.end()) {
			return errh->error("duplicate XID: ", xid_str.c_str());
		}
		// Now create a new sockaddr_in and fill it in
		XIARouteData *xrd = new XIARouteData();
		if(xrd == NULL) {
			return errh->error("Memory allocation failed");
		}
		xrd->port = port;
		xrd->flags = flags; // hardcoded to 4, representing ipv4 for now
		xrd->nexthop = NULL; // Will fill nexthop_in with the address instead
		xrd->nexthop_in = std::move(addr);
		table->_rts[xid] = xrd;
	}
	return 0;
}

int
XIACIDRouteTable::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
	XIACIDRouteTable* table = static_cast<XIACIDRouteTable*>(e);

	if (xid_str.length() == 0)
	{
		// ignore empty entry
		return 0;
	}

	if (xid_str == "-") {
		table->_rtdata.port = -1;
		table->_rtdata.flags = 0;
		if (table->_rtdata.nexthop) {
			delete table->_rtdata.nexthop;
			table->_rtdata.nexthop = NULL;
		}
		// Since nexthop_in is not going out of scope, it must be reset
		table->_rtdata.nexthop_in.reset(nullptr);

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

int
XIACIDRouteTable::load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
	std::ifstream in_f(conf.c_str());
	if (!in_f.is_open())
	{
		errh->error("could not open file: %s", conf.c_str());
		return -1;
	}

	int c = 0;
	while (!in_f.eof())
	{
		char buf[1024];
		in_f.getline(buf, sizeof(buf));

		if (strlen(buf) == 0)
			continue;

		if (set_handler(buf, e, 0, errh) != 0)
			return -1;

		c++;
	}
	click_chatter("loaded %d entries", c);

	return 0;
#elif CLICK_LINUXMODLE
	int c = 0;
	char buf[1024];

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	struct file * filp = file_open(conf.c_str(), O_RDONLY, 0);
	if (filp==NULL)
	{
		errh->error("could not open file: %s", conf.c_str());
		return -1;
	}
	loff_t file_size = vfs_llseek(filp, (loff_t)0, SEEK_END);
	loff_t curpos = 0;
	while (curpos < file_size)	{
	file_read(filp, curpos, buf, 1020);
	char * eol = strchr(buf, '\n');
	if (eol==NULL) {
			click_chatter("Error at %s %d\n", __FUNCTION__, __LINE__);
		break;
	}
	curpos+=(eol+1-buf);
		eol[1] = '\0';
		if (strlen(buf) == 0)
			continue;

		if (set_handler(buf, e, 0, errh) != 0) {
			click_chatter("Error at %s %d\n", __FUNCTION__, __LINE__);
			return -1;
	}
		c++;
	}
	set_fs(old_fs);

	click_chatter("XIA routing table loaded %d entries", c);
	return 0;
#endif
}

int
XIACIDRouteTable::generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
	XIACIDRouteTable* table = dynamic_cast<XIACIDRouteTable*>(e);
#else
	XIACIDRouteTable* table = reinterpret_cast<XIACIDRouteTable*>(e);
#endif
	assert(table);

	String conf_copy = conf;

	String xid_type_str = cp_shift_spacevec(conf_copy);
	uint32_t xid_type;
	if (!cp_xid_type(xid_type_str, &xid_type))
		return errh->error("invalid XID type: ", xid_type_str.c_str());

	String count_str = cp_shift_spacevec(conf_copy);
	int count;
	if (!cp_integer(count_str, &count))
		return errh->error("invalid entry count: ", count_str.c_str());

	String port_str = cp_shift_spacevec(conf_copy);
	int port;
	if (!cp_integer(port_str, &port))
		return errh->error("invalid port: ", port_str.c_str());

#if CLICK_USERLEVEL
	unsigned short xsubi[3];
	xsubi[0] = 1;
	xsubi[1] = 2;
	xsubi[2] = 3;
//	unsigned short xsubi_next[3];
#else
	struct rnd_state state;
	prandom32_seed(&state, 1239);
#endif

	struct click_xia_xid xid_d;
	xid_d.type = xid_type;

	if (port<0) click_chatter("Random %d ports", -port);

	for (int i = 0; i < count; i++)
	{
		uint8_t* xid = xid_d.id;
		const uint8_t* xid_end = xid + CLICK_XIA_XID_ID_LEN;
#define PURE_RANDOM
#ifdef PURE_RANDOM
		uint32_t seed = i;
		memcpy(&xsubi[1], &seed, 2);
		memcpy(&xsubi[2], &(reinterpret_cast<char *>(&seed)[2]), 2);
		xsubi[0]= xsubi[2]+ xsubi[1];
#endif

		while (xid != xid_end)
		{
#if CLICK_USERLEVEL
#ifdef PURE_RANDOM
			*reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(xsubi));
#else
			*reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(xsubi));
#endif
#else
			*reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(prandom32(&state));
			if (i%5000==0)
				click_chatter("random value %x", *reinterpret_cast<uint32_t*>(xid));
#endif
			xid += sizeof(uint32_t);
		}

		/* random generation from 0 to |port|-1 */
		XIARouteData *xrd = new XIARouteData();
		xrd->flags = 0;
		xrd->nexthop = NULL;

		if (port<0) {
#if CLICK_LINUXMODULE
			u32 random = random32();
#else
			int random = rand();
#endif
			random = random % (-port);
			xrd->port = random;
			if (i%5000 == 0)
				click_chatter("Random port for XID %s #%d: %d ",XID(xid_d).unparse_pretty(e).c_str(), i, random);
		} else
			xrd->port = port;

		table->_rts[XID(xid_d)] = xrd;
	}

	click_chatter("generated %d entries", count);
	return 0;
}


void
XIACIDRouteTable::push(int /*in_ether_port*/, Packet *p)
{
    int port;

	//in_ether_port = XIA_PAINT_ANNO(p);

	if (!_principal_type_enabled) {
		output(2).push(p);
		return;
	}

	// FIXME: delete these commented lines
    // if(in_ether_port == REDIRECT) {
    //     // if this is an XCMP redirect packet
    //     process_xcmp_redirect(p);
    //     p->kill();
    //     return;
    // } else {
    	port = lookup_route(p);
    // }

	//NITIN disable XCMP Redirect packets
	/*
    if(port == in_ether_port && in_ether_port !=DESTINED_FOR_LOCALHOST && in_ether_port !=DESTINED_FOR_DISCARD) { // need to inform XCMP that this is a redirect
	  // "local" and "discard" shouldn't send a redirect
	  Packet *q = p->clone();
	  SET_XIA_PAINT_ANNO(q, (XIA_PAINT_ANNO(q)+TOTAL_SPECIAL_CASES)*-1);
	  output(4).push(q);
    }
	*/
    if (port >= 0) {
	  SET_XIA_PAINT_ANNO(p,port);
	  output(0).push(p);
	}
	else if (port == DESTINED_FOR_LOCALHOST) {
	  // Check that intent is a CID or NCID
	  XIAHeader hdr(p->xia_header());
	  XIAPath our_path = hdr.dst_path();
	  XID intent_xid = our_path.xid(our_path.destination_node());
	  uint32_t type = ntohl(intent_xid.type());
	  if(!(type == CLICK_XIA_XID_TYPE_CID
				  || type == CLICK_XIA_XID_TYPE_NCID)) {
		  // We got a packet whose intent was not CID. Not allowed.
		  click_chatter("XIACIDRouteTable: Invalid intent for packet");
		  // TODO: Do we kill packet here or send it for rerouting?
	  }
	  // Then update destination path to <local_addr> -> XcacheSID -> CID
	  XIAPath our_new_addr = _local_addr;
	  our_new_addr.append_node(_xcache_sid);
	  our_new_addr.append_node(intent_xid);
	  XIAHeaderEncap hdr_encap(hdr);
	  hdr_encap.set_dst_path(our_new_addr);
	  // Xcache SID is the third node from end based on order of addition
	  hdr_encap.set_last(our_new_addr.unparse_node_size()-3);
	  hdr_encap.encap_replace(p);
	  // Now, send the packet up the stack
	  output(1).push(p);
	}
	else {
      // no route, feed back into the route engine
	  output(2).push(p);
    }
}

// int
// XIACIDRouteTable::process_xcmp_redirect(Packet *p)
// {
//    XIAHeader hdr(p->xia_header());
//    const uint8_t *pay = hdr.payload();
//    XID *dest, *newroute;
//    dest = new XID((const struct click_xia_xid &)(pay[4]));
//    newroute = new XID((const struct click_xia_xid &)(pay[4+sizeof(struct click_xia_xid)]));
//
//    // route update (dst, out, newroute, )
//    HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(*dest);
//    if (it != _rts.end()) {
//    	(*it).second->nexthop = newroute;
//    } else {
//        // Make a new entry for this XID
//        XIARouteData *xrd1 = new XIARouteData();
//
//        int port = _rtdata.port;
//        if(strstr(_local_addr.unparse().c_str(), dest->unparse().c_str())) {
//            port = DESTINED_FOR_LOCALHOST;
//        }
//
//        xrd1->port = port;
//        xrd1->nexthop = newroute;
//        _rts[*dest] = xrd1;
//    }
//
//    return -1;
// }

int
XIACIDRouteTable::lookup_route(Packet *p)
{
   const struct click_xia* hdr = p->xia_header();
   int last = hdr->last;
   if (last == LAST_NODE_DEFAULT)
	last = hdr->dnode - 1;
   const struct click_xia_xid_edge* edge = hdr->node[last].edge;
   const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
   const int& idx = current_edge.idx;
   if (idx == CLICK_XIA_XID_EDGE_UNUSED)
   {
	// unused edge -- use default route
  	return _rtdata.port;
    }

    const struct click_xia_xid_node& node = hdr->node[idx];

    XIAHeader xiah(p->xia_header());

	HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(node.xid);
	if (it != _rts.end())
	{
		XIARouteData *xrd = (*it).second;
		// check if outgoing packet
		if(xrd->port != DESTINED_FOR_LOCALHOST && xrd->port != FALLBACK) {
			// If nexthop is an XID, annotate the packet with it
			if(xrd->nexthop != NULL) {
				p->set_nexthop_neighbor_xid_anno(*(xrd->nexthop));
			} else if(xrd->nexthop_in != nullptr) {
				// If nexthop is an IP/port, annotate those instead
				p->set_anno_u32(DST_IP_ANNO_OFFSET,
						xrd->nexthop_in->sin_addr.s_addr);
				SET_DST_PORT_ANNO(p, xrd->nexthop_in->sin_port);
			}
		}

		return xrd->port;
	}
	else
	{
		// no match -- use default route
		// check if outgoing packet
		if(_rtdata.port != DESTINED_FOR_LOCALHOST && _rtdata.port != FALLBACK && _rtdata.nexthop != NULL) {
			p->set_nexthop_neighbor_xid_anno(*(_rtdata.nexthop));
		}
		return _rtdata.port;
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIACIDRouteTable)
ELEMENT_MT_SAFE(XIACIDRouteTable)
