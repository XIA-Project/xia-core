/*
 * xianewxidroutetable.{cc,hh} -- simple XID routing table for new XID. 
 * The code is almost the same as XIAXIDRouteTable.cc
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <click/config.h>
#include "xianewxidroutetable.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiaheader.hh>
#include <click/xiascionheader.hh>
#include <click/xiatransportheader.hh>
#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#endif
CLICK_DECLS


XIANEWXIDRouteTable::XIANEWXIDRouteTable(): _drops(0)
{
}

XIANEWXIDRouteTable::~XIANEWXIDRouteTable()
{
	_rts.clear();
}

int XIANEWXIDRouteTable::initialize(ErrorHandler *)
{
	// XLog installed the syslog error handler, use it!
	_errh = (SyslogErrorHandler*)ErrorHandler::default_handler();
	//_timer.initialize(this);
	return 0;
}

int
XIANEWXIDRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
	click_chatter("XIANEWXIDRouteTable: configuring %s\n", this->name().c_str());

	_principal_type_enabled = 1;
	_num_ports = 0;

	_rtdata.port = -1;
	_rtdata.flags = 0;
	_rtdata.nexthop = NULL;

	XIAPath local_addr;
	Element *elem;

	if (cp_va_kparse(conf, this, errh,
		"LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
		"NUM_PORT", cpkP+cpkM, cpInteger, &_num_ports,
		"HID_TABLE", cpkP+cpkM, cpElement, &elem,
		cpEnd) < 0)
	return -1;

	_local_addr = local_addr;
	_local_hid = local_addr.xid(local_addr.destination_node());
		
	_hid_table = (XIAXIDRouteTable*)elem;
	return 0;
}

int
XIANEWXIDRouteTable::set_enabled(int e)
{
	_principal_type_enabled = e;
	return 0;
}

int XIANEWXIDRouteTable::get_enabled()
{
	return _principal_type_enabled;
}

void
XIANEWXIDRouteTable::add_handlers()
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
	add_write_handler("addmtb", set_mtb_handler, 0); /*add mapping table entry*/
	//add_write_handler("setmtb", set_mtb__handler, 0); /*set mapping table entry*/
}

String
XIANEWXIDRouteTable::read_handler(Element *e, void *thunk)
{
	XIANEWXIDRouteTable *t = (XIANEWXIDRouteTable *) e;
	switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return String(t->get_enabled());

		default:
			return "<error>";
	}
}

int 
XIANEWXIDRouteTable::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
	XIANEWXIDRouteTable *t = (XIANEWXIDRouteTable *) e;
	switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return t->set_enabled(atoi(str.c_str()));

		default:
			return -1;
	}
}

String
XIANEWXIDRouteTable::list_routes_handler(Element *e, void * /*thunk */)
{
	XIANEWXIDRouteTable* table = static_cast<XIANEWXIDRouteTable*>(e);
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
XIANEWXIDRouteTable::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
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
XIANEWXIDRouteTable::set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	XIANEWXIDRouteTable* table = static_cast<XIANEWXIDRouteTable*>(e);

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
XIANEWXIDRouteTable::set_mtb_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	XIANEWXIDRouteTable* table = static_cast<XIANEWXIDRouteTable*>(e);

	bool add_mode = !thunk;

	Vector<String> args;
	int port = 0;
	unsigned flags = 0;
	String xid_str;
	XID *nexthop = NULL;

		printf("call set mtb handler - SCIONDEBUG\n");

	cp_argvec(conf, args);

		//update the code to add mapping 

	if (args.size() < 2 || args.size() > 4)
		return errh->error("invalid route: ", conf.c_str());

	xid_str = args[0];

		//printf("call set mtb handler xid_str %s - SCIONDEBUG\n", (char*)xid_str);

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
XIANEWXIDRouteTable::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
	XIANEWXIDRouteTable* table = static_cast<XIANEWXIDRouteTable*>(e);

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

		// FIXME: delete the nxthop xid if any
		table->_rts.erase(it);
		delete xrd;
	}
	return 0;
}

int
XIANEWXIDRouteTable::load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
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
XIANEWXIDRouteTable::generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
	XIANEWXIDRouteTable* table = dynamic_cast<XIANEWXIDRouteTable*>(e);
#else
	XIANEWXIDRouteTable* table = reinterpret_cast<XIANEWXIDRouteTable*>(e);
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
XIANEWXIDRouteTable::push(int in_ether_port, Packet *p)
{
	int port;

	XIAHeader xhdr(p);
	ScionHeader shdr(p);
	TransportHeader thdr(p);

	click_chatter("==== SCION FORWARD ENGINE: PUSH ====\n");
	xhdr.dump();
	shdr.dump();
	thdr.dump();

	in_ether_port = XIA_PAINT_ANNO(p);

	if (!_principal_type_enabled) {
		output(2).push(p);
		return;
	}

	click_chatter("scion: do the job\n");
	port = lookup_route(in_ether_port, p);

	click_chatter(" scion output port = %d\n", port);

	if (port >= 0) {
		SET_XIA_PAINT_ANNO(p,port);
		output(0).push(p);
	}
	else if (port == DESTINED_FOR_LOCALHOST) {
		output(1).push(p);
	}
	else {
	  //SET_XIA_PAINT_ANNO(p,UNREACHABLE);

	  //p->set_anno_u8(PAINT_ANNO_OFFSET,UNREACHABLE);

		// no match -- discard packet
	  // Output 9 is for dropping packets.
	  // let the routing engine handle the dropping.
	  //_drops++;
	  //if (_drops == 1)
	  //	  click_chatter("Dropping a packet with no match (last message)\n");
	  //  p->kill();
	  output(2).push(p);
	}
}

int
XIANEWXIDRouteTable::process_xcmp_redirect(Packet *p)
{
   XIAHeader hdr(p->xia_header());
   const uint8_t *pay = hdr.payload();
   XID *dest, *newroute;
   dest = new XID((const struct click_xia_xid &)(pay[4]));
   newroute = new XID((const struct click_xia_xid &)(pay[4+sizeof(struct click_xia_xid)]));

   // route update (dst, out, newroute, )
   HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(*dest);
   if (it != _rts.end()) {
   	(*it).second->nexthop = newroute;
   } else {
	   // Make a new entry for this XID
	   XIARouteData *xrd1 = new XIARouteData();

	   int port = _rtdata.port;
	   if(strstr(_local_addr.unparse().c_str(), dest->unparse().c_str())) {
		   port = DESTINED_FOR_LOCALHOST;
	   }

	   xrd1->port = port;
	   xrd1->nexthop = newroute;
	   _rts[*dest] = xrd1;
   }
   
   return -1;
}

int
XIANEWXIDRouteTable::lookup_route(int in_ether_port, Packet *p)
{
   DBG("scion route lookup");
   //click_chatter("scion route: lookup route return\n");
   //return DESTINED_FOR_LOCALHOST;
   const struct click_xia* hdr = p->xia_header();
   int last = hdr->last;

   if (last < 0)
	last += hdr->dnode;

   click_chatter("scion: last %d\n", last);

   const struct click_xia_xid_edge* edge = hdr->node[last].edge;
   const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
   const int& idx = current_edge.idx;
   if (idx == CLICK_XIA_XID_EDGE_UNUSED)
   {
	// unused edge -- use default route
  	return _rtdata.port;
   }

   click_chatter("scion: idx %d\n", idx);

   const struct click_xia_xid_node& node = hdr->node[idx];

   XIAHeader xiah(p->xia_header());

   //scion info is in scion ext header  
   int port = scion_forward_packet(hdr);

   click_chatter("SCION PORT=%d\n", port);

   //scion info is in scion_sid
   //int port = check_scion_info(hdr);

   //TODO: define -10 in xia.h
	if(port > -10){
		XID nexthop;
		_hid_table->next_hop(port, nexthop);
		click_chatter("nexthop = %s\n", nexthop.unparse().c_str());
		p->set_nexthop_neighbor_xid_anno(nexthop);

	} else {
		port = DESTINED_FOR_DISCARD;
	}

   //quick hack
   //if(idx == 0){
   //  DBG("scion: idx %d - return for local host\n", idx);
   //  click_chatter("chatter scion: idx %d - return for local host\n", idx);
   //  return DESTINED_FOR_LOCALHOST;
   //}

   return port;
}

/*
typedef struct {
		uint16_t versionSrcDst;
		uint16_t totalLen;
	uint8_t currentIOF;
	uint8_t currentOF;
	uint8_t nextHeader;
	uint8_t headerLen;
} SCIONCommonHeader;
*/
void XIANEWXIDRouteTable::print_scion_header(SCIONCommonHeader *sch){
	click_chatter("print scion common header:");
	click_chatter("versionSrcDst : %d", ntohs(sch->versionSrcDst));
	click_chatter("totalLen: %d", ntohs(sch->totalLen));
	click_chatter("currentIOF: %d", sch->currentIOF);
	click_chatter("currentOF: %d", sch->currentOF);
	click_chatter("nextHeader: %d", sch->nextHeader);
	click_chatter("headerLen: %d", sch->headerLen);
	return;
}

void XIANEWXIDRouteTable::print_packet_contents(uint8_t *packet, int len)
{
	int hex_string_len = (len*2) + 1;
	char hex_string[hex_string_len];
	int i;
	uint8_t* data = (uint8_t*)packet;
	bzero(hex_string, hex_string_len);
	for(i=0;i<len;i++) {
		sprintf(&hex_string[2*i], "%02x", (char)data[i]);
	}
	hex_string[hex_string_len-1] = '\0';
	click_chatter("Packet contents|%s|", hex_string);
}


int XIANEWXIDRouteTable::print_packet_header(click_xia *xiah)
{
	click_chatter("======= XIA PACKET HEADER ========");
	click_chatter("ver:%d", xiah->ver);
	click_chatter("nxt:%d", xiah->nxt);
	click_chatter("plen:%d", ntohs(xiah->plen));
	click_chatter("hlim:%d", xiah->hlim);
	click_chatter("dnode:%d", xiah->dnode);
	click_chatter("snode:%d", xiah->snode);
	click_chatter("last:%d", xiah->last);
		return xiah->dnode + xiah->snode;;
}

int XIANEWXIDRouteTable::check_scion_info(const struct click_xia* xiah) {
  uint8_t* packet = (uint8_t *)(xiah);
  uint8_t* scion_sid = ((click_xia*)packet)->node[0].xid.id;
	  
  if(scion_sid[0] == 0){
	click_chatter("SCION FE: router 0");
	scion_sid[0] = 1;
	((click_xia*)packet)->last = -1;
	return 1;
  }else{
	click_chatter("SCION FE: router 1");
	  //second router - router 1, last router
	return DESTINED_FOR_LOCALHOST;	
  }
}

int XIANEWXIDRouteTable::scion_forward_packet(const struct click_xia* xiah) {
  //XIAHeader xiah(hdr);
  uint8_t* packet = (uint8_t *)(xiah);
  int total_nodes = xiah->dnode + xiah->snode;
  uint16_t xia_hdr_len =  sizeof(struct click_xia) + total_nodes*sizeof(struct click_xia_xid_node);
  uint16_t hdr_len = xia_hdr_len;
  struct click_xia_ext* xia_ext_hdr = (struct click_xia_ext *)(packet + hdr_len);
  uint8_t nxt_hdr_type = xiah->nxt;
  uint8_t OF_offset = sizeof(SCIONCommonHeader) + sizeof(SCIONAddr) 
					   + sizeof(SCIONAddr) + sizeof(InfoOpaqueField);
  //print_packet_header((click_xia*)packet);
  click_chatter("xia_hdr_len %d, total nodes %d", xia_hdr_len, total_nodes);

  while((nxt_hdr_type != CLICK_XIA_NXT_SCION) && (nxt_hdr_type != CLICK_XIA_NXT_NO)){
	click_chatter("next header type %d, xia hdr len %d ", (int)nxt_hdr_type, hdr_len);
	nxt_hdr_type = xia_ext_hdr->nxt;
	hdr_len += xia_ext_hdr->hlen;
	xia_ext_hdr = (struct click_xia_ext *)(packet + hdr_len);
  }
  
  if(nxt_hdr_type == CLICK_XIA_NXT_SCION){
	click_chatter("get SCION header!! scion ext offset %d, header len %d", hdr_len, xia_ext_hdr->hlen);
	SCIONCommonHeader *scion_common_hdr = (SCIONCommonHeader*)((uint8_t*)xia_ext_hdr + 2);
	//uint8_t* data = (uint8_t*)scion_common_hdr;
	//HopOpaqueField* current_hops = (HopOpaqueField)((uint8_t *)scion_common_hdr + scion_common_hdr->currentOF);
	click_chatter("SCION FE: OF offset %d, currentOF %d", OF_offset, scion_common_hdr->currentOF);
	click_chatter("SCION FE: scion ext header length %d", xia_ext_hdr->hlen);
	print_scion_header(scion_common_hdr);
	print_packet_contents((uint8_t*)xia_ext_hdr, xia_ext_hdr->hlen);
 
   /*
	if(data[0] == 0){
	  click_chatter("SCION FE: router 0");
	  data[0] = 1;
	  ((click_xia*)packet)->last = -1;
	  return 1; // next router
	}else{
	  click_chatter("SCION FE: router 1");
	  //second router - router 1, last router
	  return DESTINED_FOR_LOCALHOST;
	}
	*/
	if(OF_offset == scion_common_hdr->currentOF){
	  click_chatter("SCION FE: router 0");
	  // first router - router 0
	  scion_common_hdr->currentOF += sizeof(HopOpaqueField);
	  //xiah->last = -1;
	  ((click_xia*)packet)->last = -1;
	  return 1; // next router
	}else{
	  click_chatter("SCION FE: router 1");
	  //second router - router 1, last router
	  return DESTINED_FOR_LOCALHOST;
	}
	
  }else{
	click_chatter("no SCION header!! return -10");
	return -10; //unused for SCION now 
  }
   
  return DESTINED_FOR_LOCALHOST;
}

/*
size_t add_scion_header(SCIONCommonHeader* sch, uint8_t* payload, uint16_t payload_len) {
  uint8_t *ptr;
  size_t path_length;
  size_t header_length;
  SCIONAddr src_addr, dst_addr;
  
  //data packet uses IPV4 although XIA-SCION does not use it at all.
  //it is used to distinglish data or control packet
  build_cmn_hdr(sch, ADDR_IPV4_TYPE, ADDR_IPV4_TYPE, L4_UDP);  

  //Fill in SCION addresses - 
  // we need to isd and ad information for routing.
  // but we do not need the address info
  src_addr.isd_ad = ISD_AD(my_isd, my_ad);
  dst_addr.isd_ad = ISD_AD(neighbor_isd, neighbor_ad);
  build_addr_hdr(sch, &src_addr, &dst_addr);

  //path information - gateway should call path server to get the path info
  InfoOpaqueField up_inf = {IOF_CORE, htonl(1111), htons(1), 2};
  HopOpaqueField up_hops = {HOP_NORMAL_OF, 111, IN_EGRESS_IF(12, 45), htonl(0x010203)};
  HopOpaqueField up_hops_next = {HOP_NORMAL_OF, 111, IN_EGRESS_IF(78, 98), htonl(0x010203)};
  InfoOpaqueField core_inf = {IOF_CORE, htonl(2222), htons(1), 2};
  HopOpaqueField core_hops = {HOP_NORMAL_OF, 111, htonl(IN_EGRESS_IF(11, 22)), htonl(0x010203)};
  HopOpaqueField core_hops_next = {HOP_NORMAL_OF, 111, htonl(IN_EGRESS_IF(33, 44)), htonl(0x010203)};
  InfoOpaqueField dw_inf = {IOF_CORE, htonl(3333), htons(1), 2};
  HopOpaqueField dw_hops = {HOP_NORMAL_OF, 111, htonl(IN_EGRESS_IF(12, 45)), htonl(0x010203)};
  HopOpaqueField dw_hops_next = {HOP_NORMAL_OF, 111, htonl(IN_EGRESS_IF(78, 78)), htonl(0x010203)};  

  ptr = (uint8_t *)sch + sizeof(SCIONCommonHeader) + sizeof(SCIONAddr) + sizeof(SCIONAddr);

  sch->currentOF = sizeof(SCIONCommonHeader) + sizeof(SCIONAddr) + sizeof(SCIONAddr) + sizeof(up_inf);
  
  size_t offset = 0;
  //add path info to SCION header
  memcpy(ptr, (void*)&up_inf, sizeof(up_inf));
  offset += sizeof(up_inf);
  memcpy(ptr + offset, (void*)&up_hops, sizeof(up_hops));
  offset += sizeof(up_hops);
  memcpy(ptr + offset, (void*)&up_hops_next, sizeof(up_hops_next));
  offset += sizeof(up_hops_next);
  memcpy(ptr + offset, (void*)&core_inf, sizeof(core_inf));
  offset += sizeof(core_inf);
  memcpy(ptr + offset, (void*)&core_hops, sizeof(core_hops));
  offset += sizeof(core_hops);
  memcpy(ptr + offset, (void*)&core_hops_next, sizeof(core_hops_next));
  offset += sizeof(core_hops_next);
  memcpy(ptr + offset, (void*)&dw_inf, sizeof(dw_inf));
  offset += sizeof(dw_inf);
  memcpy(ptr + offset, (void*)&dw_hops, sizeof(dw_hops));
  offset += sizeof(dw_hops);
  memcpy(ptr + offset, (void*)&dw_hops_next, sizeof(dw_hops_next));
  offset += sizeof(dw_hops_next);
  ptr += offset;
  path_length = offset;
  *(size_t *)ptr = htons(path_length); 
  path_length += 4;

  header_length = sizeof(SCIONCommonHeader) +
	sizeof(SCIONAddr) + sizeof(SCIONAddr) + path_length;
  sch->headerLen = htons(header_length);

  printf("path length is %d bytes\n", (int)path_length);
  
  memcpy(((uint8_t*)sch +  header_length), payload, payload_len);

  sch->totalLen = htons(header_length + payload_len);
  
  return header_length;
}


size_t add_scion_ext_header(click_xia_ext* xia_ext_header, uint8_t* packet_payload, uint16_t payload_len){
  SCIONCommonHeader *scion_common_header = (SCIONCommonHeader*)(&xia_ext_header->data[0]);
  size_t scion_len = add_scion_header(scion_common_header, packet_payload, payload_len);
  xia_ext_header->hlen = scion_len + 2; 
  xia_ext_header->nxt = CLICK_XIA_NXT_NO; 
  return xia_ext_header->hlen;
}
*/
//
CLICK_ENDDECLS
EXPORT_ELEMENT(XIANEWXIDRouteTable)
ELEMENT_MT_SAFE(XIANEWXIDRouteTable)
