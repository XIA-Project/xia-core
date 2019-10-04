/*
 * xiaicidroutetable.{cc,hh} -- simple XID routing table
 */

#include <click/config.h>
#include "xiaicidroutetable.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiaheader.hh>
#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#endif
CLICK_DECLS

XIAICIDRouteTable::XIAICIDRouteTable(): _drops(0)
{
}

XIAICIDRouteTable::~XIAICIDRouteTable()
{
	_rts.clear();
}

int
XIAICIDRouteTable::configure(Vector<String> & /*conf*/, ErrorHandler * /*errh*/)
{
    //click_chatter("XIAICIDRouteTable: configuring %s\n", this->name().c_str());

	_principal_type_enabled = 1;

    _rtdata.port = -1;
    _rtdata.flags = 0;
    _rtdata.nexthop = NULL;

	return 0;
}

int
XIAICIDRouteTable::set_enabled(int e)
{
	_principal_type_enabled = e;
	return 0;
}

int XIAICIDRouteTable::get_enabled()
{
	return _principal_type_enabled;
}

void
XIAICIDRouteTable::add_handlers()
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
XIAICIDRouteTable::read_handler(Element *e, void *thunk)
{
	XIAICIDRouteTable *t = (XIAICIDRouteTable *) e;
    switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return String(t->get_enabled());

		default:
			return "<error>";
    }
}

int
XIAICIDRouteTable::write_handler(const String &str, Element *e, void *thunk, ErrorHandler * errh)
{
	XIAICIDRouteTable *t = (XIAICIDRouteTable *) e;
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
			click_chatter("XIAICIDRouteTable: DAG is now %s",
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
			click_chatter("XIAICIDRouteTable: Xcache is %s",
					t->_xcache_sid.unparse().c_str());
			break;
		}
		default:
			return -1;
    }
	return 0;
}

String
XIAICIDRouteTable::list_routes_handler(Element *e, void * /*thunk */)
{
	XIAICIDRouteTable* table = static_cast<XIAICIDRouteTable*>(e);
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
XIAICIDRouteTable::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
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
XIAICIDRouteTable::set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	XIAICIDRouteTable* table = static_cast<XIAICIDRouteTable*>(e);

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
XIAICIDRouteTable::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
	XIAICIDRouteTable* table = static_cast<XIAICIDRouteTable*>(e);

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
XIAICIDRouteTable::load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
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
XIAICIDRouteTable::generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
	XIAICIDRouteTable* table = dynamic_cast<XIAICIDRouteTable*>(e);
#else
	XIAICIDRouteTable* table = reinterpret_cast<XIAICIDRouteTable*>(e);
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
XIAICIDRouteTable::push(int /*in_ether_port*/, Packet *p)
{
    /*
	uint16_t fromnet = XIA_FROMNET_ANNO(p);
	if (! (fromnet == 0 || fromnet == 1)) {
		click_chatter("XIAICIDRouteTable: Invalid fromnet!");
		output(3).push(p);
		return;
	}
    */

	if (!_principal_type_enabled) {
		output(2).push(p);
		return;
	}
    const struct click_xia* xiah = p->xia_header();
    Graph src_dag;
    src_dag.from_wire_format(xiah->snode, &xiah->node[xiah->dnode]);
    XIAPath src_path(src_dag);
    std::string xcache_sid(_xcache_sid.unparse().c_str());
    if (src_path.intent_aid_str() == xcache_sid) {
        output(2).push(p);
    } else {
        output(1).push(p);
    }

    /*
	if (fromnet == 1) {
		// Packet from network, send to API
		output(1).push(p);
	} else {
		// Packet from API, ignore *-------> ICID link, try fallback.
		output(2).push(p);
	}
    */
}

/*
XIAICIDRouteTable::lookup_route(Packet *p)
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
		if(xrd->port != DESTINED_FOR_LOCALHOST && xrd->port != FALLBACK && xrd->nexthop != NULL) {
			p->set_nexthop_neighbor_xid_anno(*(xrd->nexthop));
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
*/

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAICIDRouteTable)
ELEMENT_MT_SAFE(XIAICIDRouteTable)
