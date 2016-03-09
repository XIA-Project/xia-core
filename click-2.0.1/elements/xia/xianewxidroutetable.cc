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
//	xhdr.dump();
//	shdr.dump();
//	thdr.dump();

	in_ether_port = XIA_PAINT_ANNO(p);

	if (!_principal_type_enabled) {
		output(2).push(p);
		return;
	}

//	click_chatter("scion: do the job\n");
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
//   DBG("scion route lookup");
   //click_chatter("scion route: lookup route return\n");
   //return DESTINED_FOR_LOCALHOST;
   const struct click_xia* hdr = p->xia_header();
   int last = hdr->last;

   if (last < 0)
	last += hdr->dnode;

//   click_chatter("scion: last %d\n", last);

   const struct click_xia_xid_edge* edge = hdr->node[last].edge;
   const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
   const int& idx = current_edge.idx;
   if (idx == CLICK_XIA_XID_EDGE_UNUSED)
   {
	// unused edge -- use default route
  	return _rtdata.port;
   }

//   click_chatter("scion: idx %d\n", idx);

   const struct click_xia_xid_node& node = hdr->node[idx];

   XIAHeader xiah(p->xia_header());

   //scion info is in scion ext header  
   int port = scion_forward_packet(hdr);

   click_chatter("SCION OUTPUT PORT=%d\n", port);

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
/*
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
*/
void XIANEWXIDRouteTable::print_packet_contents(uint8_t *packet, int len)
{
	int hex_string_len = (len*2) + 1;
	char hex_string[hex_string_len];
	int i;
	uint8_t* data = (uint8_t*)packet;
	bzero(hex_string, hex_string_len);
	for(i=0;i<len;i++) {
		sprintf(&hex_string[2*i], "%02x", (unsigned int)data[i]);
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

uint16_t XIANEWXIDRouteTable::hof_get_ingress(HopOpaqueField *hof){
  return ((uint16_t)hof->ingress_egress[0]) << 4 | ((uint16_t)hof->ingress_egress[1] & 0xf0) >> 4;
}

uint16_t XIANEWXIDRouteTable::hof_get_egress(HopOpaqueField *hof){
  return ((uint16_t)hof->ingress_egress[1] & 0xf) << 8 | ((uint16_t)hof->ingress_egress[2]);
}

uint16_t XIANEWXIDRouteTable::iof_get_isd(InfoOpaqueField* iof){
  return (uint16_t)iof->isd_id[0] << 8 | iof->isd_id[1]; 
}

uint32_t XIANEWXIDRouteTable::iof_get_timestamp(InfoOpaqueField* iof){
  return ((uint32_t)iof->timestamp[0] << 24) | ((uint32_t)iof->timestamp[2] << 16) | ((uint32_t)iof->timestamp[1] << 8) | (uint32_t)iof->timestamp[0]; 
}

uint8_t XIANEWXIDRouteTable::is_on_up_path(InfoOpaqueField *currIOF) {
  if ((currIOF->info & 0x1) == 1) { // low bit of type field is used for uppath/downpath flag
    click_chatter("is on up path\n");
    return 1;
  }
  return 0;
}

bool XIANEWXIDRouteTable::is_last_path_of(SCIONCommonHeader *sch) {
  uint8_t offset = sch->headerLen -  sizeof(HopOpaqueField);
  click_chatter("is_last_path_of %d %d\n",sch->currentOF, offset);
  return sch->currentOF == offset;
}


int XIANEWXIDRouteTable::print_scion_path_info(uint8_t* path, uint32_t path_len){
  InfoOpaqueField *iof = (InfoOpaqueField *)path;
  click_chatter("Print scion path info, path length %d bytes:\n", path_len);
  click_chatter("InfoOpaqueField:\n");
  click_chatter("info %x\n", iof->info >> 1);
  click_chatter("flag %x\n", iof->info & 0x1);

  print_packet_contents((uint8_t*)iof, 8);
  click_chatter("info %#x, flag %d, isd-id %d, hops %d\n", iof->info >> 1, iof->info & 0x1, iof_get_isd(iof), iof->hops);

  for(int i = 0; i < 2; i++){
      HopOpaqueField *hof = (HopOpaqueField *)((uint8_t *)path + sizeof(InfoOpaqueField) + i * sizeof(HopOpaqueField));
      print_packet_contents((uint8_t*)hof, sizeof(HopOpaqueField));
      click_chatter("Ingress %d, Egress %d\n", hof_get_ingress(hof), hof_get_egress(hof));
  }
  return 0;
}

int XIANEWXIDRouteTable::print_scion_header(uint8_t *hdr){
    SCIONCommonHeader *sch = (SCIONCommonHeader *)hdr;
    HopOpaqueField *hof;
    InfoOpaqueField *iof;
    click_chatter("print scion common header:");

    click_chatter("versionAddrs : %d\n", ntohs(sch->versionSrcDst));
    click_chatter("totalLen: %d\n", ntohs(sch->totalLen));
    click_chatter("currentIOF: %d\n", sch->currentIOF);
    click_chatter("currentOF: %d\n", sch->currentOF);
    click_chatter("nextHeader: %d\n", sch->nextHeader);
    click_chatter("headerLen: %d\n", sch->headerLen);
    
    iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);
    print_packet_contents((uint8_t*)iof, 8);
    click_chatter("info %#x, flag %d, isd-id %d, hops %d\n", iof->info >> 1, iof->info & 0x1, iof_get_isd(iof), iof->hops);

    for(int i = 0; i < 2; i++){
      hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentIOF + sizeof(InfoOpaqueField) + i * sizeof(HopOpaqueField));
      print_packet_contents((uint8_t*)hof, sizeof(HopOpaqueField));
      click_chatter("info is %#x, Ingress %d, Egress %d\n", hof->info, hof_get_ingress(hof), hof_get_egress(hof));
    }
    
    return 0;
}

int XIANEWXIDRouteTable::handle_ingress_xovr(SCIONCommonHeader *sch) {
  HopOpaqueField *hof;
  InfoOpaqueField *iof;

  click_chatter("handle ingress xovr\n");
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);
  click_chatter("iof->info %d\n", iof->info);

  if (iof->info >>1 == IOF_SHORTCUT) {
    return ingress_shortcut_xovr(sch);
  } else if (iof->info >>1 == IOF_INTRA_ISD_PEER ||
             iof->info >>1 == IOF_INTER_ISD_PEER) {
    return ingress_peer_xovr(sch);
  } else if (iof->info >>1 == IOF_CORE) {
    return ingress_core_xovr(sch);
  } else {
    click_chatter("Invalid iof->info %d\n", iof->info);
    return -10;
  }
  return -10;
}

int XIANEWXIDRouteTable::ingress_shortcut_xovr(SCIONCommonHeader *sch) {
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  click_chatter("ingress shortcut xovr\n");
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  prev_hof = hof + 1;
  if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
    return -10;
  }

  // switch to next segment
  sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField) * 2;
  sch->currentOF += sizeof(HopOpaqueField) * 4;

  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  if (hof_get_ingress(hof) == 0 && is_last_path_of(sch)) {
    prev_hof = hof - 1;
    if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
      return -10;
    }
    //deliver(m, DATA_PACKET, dpdk_rx_port);
    return DESTINED_FOR_LOCALHOST;
  } else {
    return hof_get_egress(hof);
    //send_ingress(m, EGRESS_IF(hof), dpdk_rx_port);
  }
}

int XIANEWXIDRouteTable::ingress_peer_xovr(SCIONCommonHeader *sch) {
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  click_chatter("ingress peer xovr\n");
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  uint16_t fwd_if;
  if (is_on_up_path(iof)) {
    prev_hof = hof + 2; // why + 2?
    fwd_if = hof_get_ingress(hof + 1);
  } else {
    prev_hof = hof + 1; // why + 1?
    fwd_if = hof_get_egress(hof + 1);
  }

  if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
    return -10;
  }

  sch->currentOF += sizeof(HopOpaqueField);

  if (is_last_path_of(sch))
    //deliver(m, DATA_PACKET, dpdk_rx_port);
    return DESTINED_FOR_LOCALHOST;
  else
  //send_ingress(m, fwd_if, dpdk_rx_port);
    return fwd_if;
}

int XIANEWXIDRouteTable::ingress_core_xovr(SCIONCommonHeader *sch) {
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  click_chatter("ingress core xovr\n");
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  uint32_t fwd_if;
  if (is_on_up_path(iof)) {
    prev_hof = NULL;
  } else {
    prev_hof = hof - 1;
  }

  if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
    return -10;
  }

  if (is_last_path_of(sch)){
    click_chatter("is the last path\n");
    return DESTINED_FOR_LOCALHOST;
  }else {
    // Switch to next path segment
    click_chatter("switch to next segment\n");
    sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField);
    sch->currentOF += sizeof(HopOpaqueField) * 2;
    iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);
    hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);

    if (is_on_up_path(iof)) {
      //send_ingress(m, INGRESS_IF(hof), dpdk_rx_port);
      return hof_get_ingress(hof);
    } else {
      //send_ingress(m, EGRESS_IF(hof), dpdk_rx_port);
      return hof_get_egress(hof);
    }
  }

  return -10;
}

int XIANEWXIDRouteTable::ingress_normal_forward(SCIONCommonHeader *sch) {
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  click_chatter("ingress normal forward\n");
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  click_chatter("Ingress %d, Egress %d\n", hof_get_ingress(hof), hof_get_egress(hof));
  uint16_t next_ifid;
  if (is_on_up_path(iof)) {
    next_ifid = hof_get_ingress(hof);
    prev_hof = hof + 1;
  } else {
    next_ifid = hof_get_egress(hof);
    prev_hof = hof - 1;
  }

  click_chatter("Next ifid %d\n", next_ifid);

  if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
    return -10;
  }

  if (next_ifid == 0 && is_last_path_of(sch)) {
    //deliver(m, DATA_PACKET, dpdk_rx_port);
    return DESTINED_FOR_LOCALHOST;    
  } else {
    //send_ingress(m, next_ifid, dpdk_rx_port);
    return next_ifid;
  }

  return -10;
}

int XIANEWXIDRouteTable::handle_egress_xovr(SCIONCommonHeader *sch) {
  HopOpaqueField *hof;
  InfoOpaqueField *iof;

  click_chatter("handle egress xovr\n");
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  if (iof->info >>1 == IOF_SHORTCUT) {
    return egress_shortcut_xovr(sch);
  } else if (iof->info >>1 == IOF_INTRA_ISD_PEER ||
             iof->info  >>1== IOF_INTER_ISD_PEER) {
    return egress_peer_xovr(sch);
  } else if (iof->info  >>1== IOF_CORE) {
    return egress_core_xovr(sch);
  } else {
    // invalid OF
    click_chatter("Invalid iof->info %#x\n", iof->info);
  }
  return -10;
}

int XIANEWXIDRouteTable::egress_shortcut_xovr(SCIONCommonHeader *sch) {

  click_chatter("egress_shortcut_xovr\n");
  return egress_normal_forward(sch);
}

int XIANEWXIDRouteTable::egress_peer_xovr(SCIONCommonHeader *sch) {
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  click_chatter("egress_peer_xovr \n");
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  if (is_on_up_path(iof)) {
    prev_hof = hof - 1;
    if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
      return -10;
    }
    // Switch to next segment
    sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField) * 2;
    sch->currentOF += sizeof(HopOpaqueField) * 4; // why *4?
  } else {
    prev_hof = hof - 2;
    if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
      return -10;
    }
    sch->currentOF += sizeof(HopOpaqueField); // why not *4?
  }
  // now we do not support send_egress
  //send_egress(m, dpdk_rx_port);
  return -10;
}

int XIANEWXIDRouteTable::egress_core_xovr(SCIONCommonHeader *sch){
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  click_chatter("egress core xovr\n");
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  if (is_on_up_path(iof)) {
    prev_hof = NULL;
  } else {
    prev_hof = hof + 1;
  }

  if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
    return -10;
  }

  sch->currentOF += sizeof(HopOpaqueField);
  
  //send_egress send a packet to a neighbor AD router? check it
  //send_egress(m, dpdk_rx_port);
  return -10;
}

int XIANEWXIDRouteTable::egress_normal_forward(SCIONCommonHeader *sch) {  
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  click_chatter("egress normal forward\n");
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

  if (is_on_up_path(iof)) {
    prev_hof = hof + 1;
  } else {
    prev_hof = hof - 1;
  }

  if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
    return -10;
  }

  sch->currentOF += sizeof(HopOpaqueField);

  // send packet to neighbor AD's router
  //send_egress(m, dpdk_rx_port);
  return -10;
}

//
//skip verification now as the routers do not have mac key
//
int XIANEWXIDRouteTable::verify_of(HopOpaqueField *hof, HopOpaqueField *prev_hof, uint32_t ts) {
  return 1;
}

int XIANEWXIDRouteTable::scion_forward_packet(const struct click_xia* xiah) {
  //XIAHeader xiah(hdr);
  uint8_t* packet = (uint8_t *)(xiah);
  int total_nodes = xiah->dnode + xiah->snode;
  uint16_t xia_hdr_len =  sizeof(struct click_xia) + total_nodes*sizeof(struct click_xia_xid_node);
  uint16_t hdr_len = xia_hdr_len;
  struct click_xia_ext* xia_ext_hdr = (struct click_xia_ext *)(packet + hdr_len);
  uint8_t nxt_hdr_type = xiah->nxt;

  while((nxt_hdr_type != CLICK_XIA_NXT_SCION) && (nxt_hdr_type != CLICK_XIA_NXT_NO)){
	click_chatter("next header type %d, xia hdr len %d ", (int)nxt_hdr_type, hdr_len);
	nxt_hdr_type = xia_ext_hdr->nxt;
	hdr_len += xia_ext_hdr->hlen;
	xia_ext_hdr = (struct click_xia_ext *)(packet + hdr_len);
  }
  
  if(nxt_hdr_type == CLICK_XIA_NXT_SCION){
	print_packet_contents((uint8_t*)xia_ext_hdr, xia_ext_hdr->hlen);
	SCIONCommonHeader *sch = (SCIONCommonHeader*)((uint8_t*)xia_ext_hdr + 2);
	print_scion_header((uint8_t*)sch);
  	uint8_t srcLen = SCION_ISD_AD_LEN + SCION_ADDR_LEN;
	uint8_t dstLen = SCION_ISD_AD_LEN + SCION_ADDR_LEN;
        uint8_t *path = (uint8_t*)sch + sizeof(SCIONCommonHeader) + srcLen + dstLen;
        uint8_t path_length = sch->headerLen - sizeof(SCIONCommonHeader) - srcLen - dstLen;

	print_scion_path_info(path, path_length);

	InfoOpaqueField* iof = (InfoOpaqueField*)((uint8_t*)sch + sch->currentIOF);

        HopOpaqueField *hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);

        click_chatter("forward_packet: hof->info=%#x, currentIOF %d, currentOF %d\n", hof->info, sch->currentIOF, sch->currentOF);
        click_chatter("egress interface %d", hof_get_egress(hof));
        
#if 1
        // in XIA-SCION, we do not support local_ad/socket now,  
        // so we only use egress interface for routing now.
        // todo: support from_local_ad
        //if(!is_last_path_of(sch)){
	if(sch->currentOF < sch->headerLen){
	  int egress_interface = hof_get_egress(hof);
	  sch->currentOF += sizeof(HopOpaqueField);
	  click_chatter("return_egress interface %d", egress_interface);
	  return egress_interface; 
	}else{
	  return DESTINED_FOR_LOCALHOST;
	}
#else
        // now per router per AD, so never from local socket/AD
        uint8_t from_local_ad = 1; 
        if (from_local_ad == 0) {
	  if (hof->info == XOVR_POINT) {
	    return handle_ingress_xovr(sch);
	  } else {
	    return ingress_normal_forward(sch);
	  }
	} else {
	  if (hof->info == XOVR_POINT) {
	    return handle_egress_xovr(sch);
	  } else {
	    return egress_normal_forward(sch);
	  }
       }
#endif	
  }else{
	click_chatter("no SCION header!! return -10");
	return -10; //unused for SCION now 
  }
   
  return DESTINED_FOR_LOCALHOST;
}


/*
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

  uint8_t* scion_path = packet + sizeof(SCIONCommonHeader) + sizeof(SCIONAddr) + sizeof(SCIONAddr);

  while((nxt_hdr_type != CLICK_XIA_NXT_SCION) && (nxt_hdr_type != CLICK_XIA_NXT_NO)){
	click_chatter("next header type %d, xia hdr len %d ", (int)nxt_hdr_type, hdr_len);
	nxt_hdr_type = xia_ext_hdr->nxt;
	hdr_len += xia_ext_hdr->hlen;
	xia_ext_hdr = (struct click_xia_ext *)(packet + hdr_len);
  }
  
  if(nxt_hdr_type == CLICK_XIA_NXT_SCION){
	SCIONCommonHeader *scion_common_hdr = (SCIONCommonHeader*)((uint8_t*)xia_ext_hdr + 2);
	print_scion_header(scion_common_hdr);
	print_packet_contents((uint8_t*)xia_ext_hdr, xia_ext_hdr->hlen);

	if(OF_offset == scion_common_hdr->currentOF){
	  // first router - router 0
	  scion_common_hdr->currentOF += sizeof(HopOpaqueField);
	  //xiah->last = -1;
	  ((click_xia*)packet)->last = -1;
	  click_chatter("in host 0, return 0 for router 0");
	  return 0; // HACK! The first hop is a host which only has one port, so send it out 0
	}else if(scion_common_hdr->currentOF == (OF_offset + sizeof(HopOpaqueField))){
          scion_common_hdr->currentOF += sizeof(HopOpaqueField);
	  //xiah->last = -1;
	  ((click_xia*)packet)->last = -1;
          click_chatter("in router 0, return port 1 for router 1");
	  return 1;
        }else if(scion_common_hdr->currentOF == (OF_offset + 2*sizeof(HopOpaqueField))){
          scion_common_hdr->currentOF += sizeof(HopOpaqueField);
	  //xiah->last = -1;
          click_chatter("in router 1, return local host for destionation host");
          return DESTINED_FOR_LOCALHOST;
	  //((click_xia*)packet)->last = -1;
          //click_chatter("in router 1, return port 0 for host 1");
	  //return 0;
	}else{
//	  click_chatter("SCION FE: router 1");
	  //second router - router 1, last router
          click_chatter("in host 1, return port local host for destination host");
	  return DESTINED_FOR_LOCALHOST;
	}
	
  }else{
	click_chatter("no SCION header!! return -10");
	return -10; //unused for SCION now 
  }
   
  return DESTINED_FOR_LOCALHOST;
}
*/
//
CLICK_ENDDECLS
EXPORT_ELEMENT(XIANEWXIDRouteTable)
ELEMENT_MT_SAFE(XIANEWXIDRouteTable)
