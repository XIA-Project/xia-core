/*
 * xiaxidroutetable.{cc,hh} -- simple XID routing table
 */

#include <click/config.h>
#include "xiaxidroutetable.hh"
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

XIAXIDRouteTable::XIAXIDRouteTable(): _drops(0)
{
}

XIAXIDRouteTable::~XIAXIDRouteTable()
{
	_rts.clear();
}

int
XIAXIDRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
    click_chatter("XIAXIDRouteTable: configuring %s\n", this->name().c_str());

	_num_ports = 0;

    _rtdata.port = -1;
    _rtdata.flags = 0;
    _rtdata.nexthop = NULL;

    XIAPath local_addr;

    if (cp_va_kparse(conf, this, errh,
		"LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
		"NUM_PORT", cpkP+cpkM, cpInteger, &_num_ports,
		cpEnd) < 0)
	return -1;

    _local_addr = local_addr;
    _local_hid = local_addr.xid(local_addr.destination_node());
        
    String broadcast_xid("HID:1111111111111111111111111111111111111111");  // broadcast HID
    _bcast_xid.parse(broadcast_xid);    
    
	return 0;
}

void
XIAXIDRouteTable::add_handlers()
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
}

String
XIAXIDRouteTable::list_routes_handler(Element *e, void * /*thunk */)
{
	XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);
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
XIAXIDRouteTable::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
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
XIAXIDRouteTable::set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);

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
XIAXIDRouteTable::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
	XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);

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
XIAXIDRouteTable::load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
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
XIAXIDRouteTable::generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
	XIAXIDRouteTable* table = dynamic_cast<XIAXIDRouteTable*>(e);
#else
	XIAXIDRouteTable* table = reinterpret_cast<XIAXIDRouteTable*>(e);
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
XIAXIDRouteTable::push(int in_ether_port, Packet *p)
{
    int port;

	in_ether_port = XIA_PAINT_ANNO(p);

    if(in_ether_port == REDIRECT) {
	  // if this is an XCMP redirect packet
	  port = process_xcmp_redirect(p);
    } else {    
    	port = lookup_route(in_ether_port, p);
    }

    if(port == in_ether_port && in_ether_port !=DESTINED_FOR_LOCALHOST && in_ether_port !=DESTINED_FOR_DISCARD) { // need to inform XCMP that this is a redirect
	  // ports 4 and 5 are "local" and "discard" so we shouldn't send a redirect in that case
	  Packet *q = p->clone();
	  SET_XIA_PAINT_ANNO(q, (XIA_PAINT_ANNO(q)+TOTAL_SPECIAL_CASES)*-1);
	  output(DESTINED_FOR_LOCALHOST).push(q); // This is not right....
    }
	SET_XIA_PAINT_ANNO(p,port);
    if (port >= 0) output(0).push(p);
	else if (port == DESTINED_FOR_LOCALHOST) output(1).push(p);
	else if (port == DESTINED_FOR_DHCP) output(3).push(p);
	else if (port == DESTINED_FOR_BROADCAST) {
	  for(int i = 0; i <= _num_ports; i++) {
		Packet *q = p->clone();
		SET_XIA_PAINT_ANNO(q,i);
		//q->set_anno_u8(PAINT_ANNO_OFFSET,i);
		output(0).push(q);
	  }
	  p->kill();
	}
	else {
	  SET_XIA_PAINT_ANNO(p,UNREACHABLE);
	  //p->set_anno_u8(PAINT_ANNO_OFFSET,UNREACHABLE);

        // no match -- discard packet
	  // Output 9 is for dropping packets.
	  // let the routing engine handle the dropping.
	  //_drops++;
	  //if (_drops == 1)
      //      click_chatter("Dropping a packet with no match (last message)\n");
      //  p->kill();
	  output(2).push(p);
    }
}

int
XIAXIDRouteTable::process_xcmp_redirect(Packet *p)
{
   XIAHeader hdr(p->xia_header());
   const uint8_t *pay = hdr.payload();
   XID *newroute;
   XIAHeader *badhdr;
   newroute = new XID((const struct click_xia_xid &)(pay[4]));
   badhdr = new XIAHeader((const struct click_xia*)(&pay[4+sizeof(struct click_xia_xid)]));
   //XIAHeader xiah(badhdr);

   XIAPath dst_path = badhdr->dst_path();
   XID dst_xid = dst_path.xid(dst_path.destination_node());

   // route update (dst, out, newroute, )
   HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(dst_xid);
   if (it != _rts.end())
   {
   	(*it).second->nexthop = newroute;
   }
   else
   {
    	// Make a new entry for this XID
       	XIARouteData *xrd1 = new XIARouteData();
	xrd1->port = _rtdata.port;
	xrd1->nexthop = newroute;
	_rts[dst_xid] = xrd1;
   }   

   //delete newroute;
   delete badhdr;
   
   return -1;
}

int
XIAXIDRouteTable::lookup_route(int in_ether_port, Packet *p)
{
   const struct click_xia* hdr = p->xia_header();
   int last = hdr->last;
   if (last < 0)
	last += hdr->dnode;
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
    
    if (_bcast_xid == node.xid) {
    	// Broadcast packet
    	
    	XIAPath source_path = xiah.src_path();
    	source_path.remove_node(source_path.destination_node());
    	XID source_hid = source_path.xid(source_path.destination_node());
    	
    	if(_local_hid == source_hid) {
    	    	// Case 1. Outgoing broadcast packet: send it to port 7 (which will duplicate the packet and send each to every interface)
    	    	p->set_nexthop_neighbor_xid_anno(_bcast_xid);
    	    	return DESTINED_FOR_BROADCAST;
    	} else {
    		// Case 2. Incoming broadcast packet: send it to port 4 (which eventually send the packet to upper layer)
    		// Also, mark the incoming (ethernet) interface number that connects to this neighbor
    		HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(source_hid);
    		if (it != _rts.end())
			  {
				if ((*it).second->port != in_ether_port) {
				  // update the entry
				  (*it).second->port = in_ether_port;
				}	
			  }
    		else
			  {
    			// Make a new entry for this newly discovered neighbor
       			XIARouteData *xrd1 = new XIARouteData();
				xrd1->port = in_ether_port;
				xrd1->nexthop = new XID(source_hid);
				_rts[source_hid] = xrd1;
			  }
    		return DESTINED_FOR_LOCALHOST;
    	}    	
       return 0;
    
    } else {
    	// Unicast packet
		HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(node.xid);
		if (it != _rts.end())
		{
			XIARouteData *xrd = (*it).second;
			// check if outgoing packet
			if(xrd->port != 4 && xrd->port != 5 && xrd->nexthop != NULL) {
				p->set_nexthop_neighbor_xid_anno(*(xrd->nexthop));
			}
			return xrd->port;
		}
		else
		{
			// no match -- use default route
			// check if outgoing packet
			if(_rtdata.port != 4 && _rtdata.port != 5 && _rtdata.nexthop != NULL) {
				p->set_nexthop_neighbor_xid_anno(*(_rtdata.nexthop));
			}			
			return _rtdata.port;
		}
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAXIDRouteTable)
ELEMENT_MT_SAFE(XIAXIDRouteTable)
