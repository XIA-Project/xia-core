/*
 * xiaaidroutetable.{cc,hh} -- simple XID routing table
 */

#include <click/config.h>
#include "xiaaidroutetable.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiaheader.hh>
#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#include <arpa/inet.h>
#endif
CLICK_DECLS

#define DATA_PORT         0
#define REGISTRATION_PORT 1

XIAAIDRouteTable::XIAAIDRouteTable()//: _drops(0)
{
	_drops = 0;
}

XIAAIDRouteTable::~XIAAIDRouteTable()
{
	_rts.clear();
	if(_sockfd != -1) {
		close(_sockfd);
	}
}

int
XIAAIDRouteTable::configure(Vector<String> & /*conf*/, ErrorHandler * /*errh*/)
{
    //click_chatter("XIAAIDRouteTable: configuring %s\n", this->name().c_str());

	_principal_type_enabled = 1;

    _rtdata.port = -1;
    _rtdata.flags = 0;
    _rtdata.nexthop = NULL;

	_sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	return 0;
}

int
XIAAIDRouteTable::set_enabled(int e)
{
	_principal_type_enabled = e;
	return 0;
}

int XIAAIDRouteTable::get_enabled()
{
	return _principal_type_enabled;
}

void
XIAAIDRouteTable::add_handlers()
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

/*
String
XIAAIDRouteTable::read_handler(Element *e, void *thunk)
{
	XIAAIDRouteTable *t = (XIAAIDRouteTable *) e;
    switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return String(t->get_enabled());

		default:
			return "<error>";
    }
}

int
XIAAIDRouteTable::write_handler(const String &str, Element *e, void *thunk, ErrorHandler * errh)
{
	XIAAIDRouteTable *t = (XIAAIDRouteTable *) e;
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
			click_chatter("XIAAIDRouteTable: DAG is now %s",
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
			click_chatter("XIAAIDRouteTable: Xcache is %s",
					t->_xcache_sid.unparse().c_str());
			break;
		}
		default:
			return -1;
    }
	return 0;
}
*/

String
XIAAIDRouteTable::addr_str(struct sockaddr_in* addr)
{
	String addrstr = "";
	if(addr != NULL) {
		char addrbuf[512];
		if(inet_ntop(AF_INET, addr, addrbuf, sizeof(addrbuf))) {
			click_chatter("XIAAIDRouteTable: ERROR converting addr to string");
		} else {
			addrstr += String(addrbuf);
		}
	}
	return addrstr;
}

String
XIAAIDRouteTable::list_routes_handler(Element *e, void * /*thunk */)
{
	XIAAIDRouteTable* table = static_cast<XIAAIDRouteTable*>(e);
	XIAAIDRouteData *xrd = &table->_rtdata;

	// get the default route
	String tbl = "-," + String(xrd->port) + ","
		+ table->addr_str(xrd->nexthop) + "," + String(xrd->flags) + "\n";

	// get the rest
	for(const auto& it : table->_rts) {
		String xid = it.first.unparse();
		xrd = it.second;

		tbl += xid + ",";
		tbl += String(xrd->port) + ",";
		tbl += table->addr_str(xrd->nexthop) + ",";
		tbl += String(xrd->flags) + "\n";
	}
	return tbl;
}

/*
int
XIAAIDRouteTable::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
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
XIAAIDRouteTable::set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	XIAAIDRouteTable* table = static_cast<XIAAIDRouteTable*>(e);

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
XIAAIDRouteTable::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
	XIAAIDRouteTable* table = static_cast<XIAAIDRouteTable*>(e);

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
*/

int
XIAAIDRouteTable::load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
	click_chatter("ERROR: Loading routes not supported for AID");
	errh->error("loading routes not supported for AID");
	return -1;
	/*
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
*/
}

int
XIAAIDRouteTable::generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
	click_chatter("ERROR: Generate AID routes not supported");
	errh->error("AID route generation not supported");
	return -1;
	/*
#if CLICK_USERLEVEL
	XIAAIDRouteTable* table = dynamic_cast<XIAAIDRouteTable*>(e);
#else
	XIAAIDRouteTable* table = reinterpret_cast<XIAAIDRouteTable*>(e);
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
		*/

		/* random generation from 0 to |port|-1 */
	/*
		XIAAIDRouteData *xrd = new XIAAIDRouteData();
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
	*/
}

void
XIAAIDRouteTable::ProcessRegistration(WritablePacket* p_in)
{
	click_chatter("XIAAIDRouteTable: processing registration request");
	uint8_t *data = p_in->data();
	if(data[0] != 0xc0 || data[1] != 0xda) {
		click_chatter("XIAAIDRouteTable: Invalid registration packet");
		return;
	}
	click_chatter("XIAAIDRouteTable: found valid magic number");
	size_t offset = 2; // past the 0xc0da magic number
	click_chatter("XIAAIDRouteTable: AID len should be %d", data[offset]);
	// Get AID length and AID
	int aidlen = data[offset++];
	click_chatter("XIAAIDRouteTable: AID len is %d", aidlen);
	char* aidstr = (char*) &data[offset];
	offset += aidlen;
	// Get address length and address
	click_chatter("XIAAIDRouteTable: addr len should be %d", data[offset]);
	int addrlen = data[offset++];
	click_chatter("XIAAIDRouteTable: addr len is %d", addrlen);
	struct sockaddr_storage aid_addr;
	memcpy(&aid_addr, &data[offset], addrlen);
	// verify that the address is an IP address
	/*
	if(ntohs(aid_addr.ss_family) != AF_INET) {
		click_chatter("XIAAIDRouteTable: ERROR invalid addr for registration");
		return;
	}
	*/
	struct sockaddr_in* aid_addrptr = (struct sockaddr_in*) &aid_addr;
	click_chatter("XIAAIDRouteTable: Got address %s",
			inet_ntoa(aid_addrptr->sin_addr));
	click_chatter("XIAAIDRouteTable: port %d", ntohs(aid_addrptr->sin_port));
	if(addrlen != sizeof(sockaddr_in)) {
		click_chatter("XIAAIDRouteTable: ERROR addr size is invalid");
		return;
	}
	String xidstr(aidstr, aidlen);
	click_chatter("XIAAIDRouteTable: registering %s", xidstr.c_str());
	XID xid(xidstr);
	// Check if an entry exists in forwarding table already
	auto it = _rts.find(xid);
	if(it != _rts.end()) {
		delete it->second;
		click_chatter("XIAAIDRouteTable: Replacing table entry %s",
				it->first.unparse().c_str());
	}
	XIAAIDRouteData* routedata = new XIAAIDRouteData();
	routedata->port = -1;
	routedata->flags = 0;
	routedata->nexthop = (struct sockaddr_in*) calloc(
			1, sizeof(struct sockaddr_in));
	memcpy(routedata->nexthop, &aid_addr, addrlen);
	_rts[xid] = routedata;
	click_chatter("XIAAIDRouteTable: size: %zu\n", _rts.size());
}

void
XIAAIDRouteTable::ProcessAIDPacket(WritablePacket* p_in)
{
	// Read the XIA Header
	XIAHeader xiah(p_in->xia_header());
	int next = xiah.nxt();
	if (next != CLICK_XIA_NXT_DATA) {
		click_chatter("XIAAIDRouteTable: AID packet invalid");
		return;
	}
	XIAPath dst_path = xiah.dst_path();
	std::string aid_str = dst_path.intent_aid_str();
	if (aid_str.size() < 20) {
		click_chatter("XIAAIDRouteTable: Intent AID invalid");
		return;
	}
	String aidStr(aid_str.data(), aid_str.size());
	XID aid(aidStr);
	// If found in table, send packet along via IP socket
	auto it = _rts.find(aid);
	auto routedata = it->second;
	if(routedata->nexthop == NULL) {
		click_chatter("XIAAIDRouteTable: found entry but nexthop invalid");
		return;
	}
	// TODO: send packet to the intended QUIC XIA Application
	int retval = sendto(_sockfd, p_in->data(), p_in->length(), 0,
			(struct sockaddr*)routedata->nexthop, sizeof(struct sockaddr_in));
	if(retval == -1) {
		click_chatter("XIAAIDRouteTable: Unable to send packet along");
		return;
	}
	click_chatter("XIAAIDRouteTable: forwarded packet to %s:%d",
			inet_ntoa(routedata->nexthop->sin_addr),
			ntohs(routedata->nexthop->sin_port));
}

void
XIAAIDRouteTable::push(int port, Packet *p)
{
	WritablePacket *p_in = p->uniqueify();
	switch(port) {
		case DATA_PORT:
			ProcessAIDPacket(p_in);
			break;
		case REGISTRATION_PORT:
			ProcessRegistration(p_in);
			break;
		default:
			click_chatter("packet from unknown port: %d\n", port);
			break;
	}
	p_in->kill();
	/*
	uint16_t fromnet = XIA_FROMNET_ANNO(p);
	if (! (fromnet == 0 || fromnet == 1)) {
		click_chatter("XIAAIDRouteTable: Invalid fromnet!");
		output(3).push(p);
		return;
	}

	if (!_principal_type_enabled) {
		output(2).push(p);
		return;
	}

	if (fromnet == 1) {
		// Packet from network, send to API
		output(1).push(p);
	} else {
		// Packet from API, ignore *-------> AID link, try fallback.
		output(2).push(p);
	}
	*/
}

int
XIAAIDRouteTable::lookup_route(Packet *p)
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

	auto it = _rts.find(node.xid);
	if (it != _rts.end())
	{
		XIAAIDRouteData *xrd = (*it).second;
		// check if outgoing packet
		if(xrd->port != DESTINED_FOR_LOCALHOST && xrd->port != FALLBACK && xrd->nexthop != NULL) {
			click_chatter("Sending packet along to QUIC XIA application");
			//p->set_nexthop_neighbor_xid_anno(*(xrd->nexthop));
		}
		return xrd->port;
	}
	else
	{
		// no match -- use default route
		// check if outgoing packet
		if(_rtdata.port != DESTINED_FOR_LOCALHOST && _rtdata.port != FALLBACK && _rtdata.nexthop != NULL) {
			// TODO: NITIN figure out what we should do here.
			click_chatter("Figure out what needs done here");
			//p->set_nexthop_neighbor_xid_anno(*(_rtdata.nexthop));
		}
		return _rtdata.port;
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAAIDRouteTable)
ELEMENT_MT_SAFE(XIAAIDRouteTable)
