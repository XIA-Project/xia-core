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

    if (cp_va_kparse(conf, this, errh,
		"LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
		"NUM_PORT", cpkP+cpkM, cpInteger, &_num_ports,
		cpEnd) < 0)
	return -1;

    _local_addr = local_addr;
    _local_hid = local_addr.xid(local_addr.destination_node());
        
    String broadcast_xid(BHID);  // broadcast HID
    _bcast_xid.parse(broadcast_xid);    
    
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

	in_ether_port = XIA_PAINT_ANNO(p);

	click_chatter("scion:newidrouter push\n");
	if (!_principal_type_enabled) {
		output(2).push(p);
		return;
	}

	click_chatter("scion: do the job\n");
    if(in_ether_port == REDIRECT) {
        // if this is an XCMP redirect packet
        process_xcmp_redirect(p);
        p->kill();
        return;
    } else {    
    	port = lookup_route(in_ether_port, p);
    }

    if(port == in_ether_port && in_ether_port !=DESTINED_FOR_LOCALHOST && in_ether_port !=DESTINED_FOR_DISCARD) { // need to inform XCMP that this is a redirect
	  // "local" and "discard" shouldn't send a redirect
	  Packet *q = p->clone();
	  SET_XIA_PAINT_ANNO(q, (XIA_PAINT_ANNO(q)+TOTAL_SPECIAL_CASES)*-1);
	  output(4).push(q); 
    }
    if (port >= 0) {
	  SET_XIA_PAINT_ANNO(p,port);
	  output(0).push(p);
	}
	else if (port == DESTINED_FOR_LOCALHOST) {
	  output(1).push(p);
	}
	else if (port == DESTINED_FOR_DHCP) {
	  SET_XIA_PAINT_ANNO(p,port);
	  output(3).push(p);
	}
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
	  //SET_XIA_PAINT_ANNO(p,UNREACHABLE);

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
		// TODO: not sure what this should be??
		assert(0);
		return DESTINED_FOR_LOCALHOST; 
    
    } else {
    	// Unicast packet
		HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(node.xid);
		if (it != _rts.end())
		{
		/* GENI TUTORIAL: EDIT BEGIN
		 * route entry found
		 * start forwarding
		 */
			XIARouteData *xrd = (*it).second;
			int port = xrd->port;
			// check if outgoing packet
			if(xrd->port != DESTINED_FOR_LOCALHOST && xrd->port != FALLBACK && xrd->nexthop != NULL) {
				p->set_nexthop_neighbor_xid_anno(*(xrd->nexthop));
			}
			return port;
		/* GENI TUTORIAL: EDIT END
		 */
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
}

int XIANEWXIDRouteTable::scion_init(int port) {
  // fill interface list
  // TODO read topology configuration
  int test = 0;
  /* 
  beacon_servers[0] = rte_cpu_to_be_32(IPv4(7, 7, 7, 7));
  certificate_servers[0] = rte_cpu_to_be_32(IPv4(8, 8, 8, 8));
  path_servers[0] = rte_cpu_to_be_32(IPv4(9, 9, 9, 9));

  // first router
  neighbor_ad_router_ip[0] = neighbor_ad_router_ip[1] =
      rte_cpu_to_be_32(IPv4(1, 1, 1, 1));
  // DPDK setting
  */
  port_map[0].egress = 0;
  port_map[0].local = 1;
  port_map[1].egress = 0;
  port_map[1].local = 1;
  my_ifid[0] = my_ifid[1] = 123; // ifid of NIC 0 and NIC 1 is 123

  // second router
  /*
  neighbor_ad_router_ip[2] = neighbor_ad_router_ip[3] =
      rte_cpu_to_be_32(IPv4(1, 1, 1, 1));
  */
  port_map[2].egress = 2;
  port_map[2].local = 3;
  port_map[3].egress = 2;
  port_map[3].local = 3;
  my_ifid[2] = my_ifid[3] = 345; // ifid of NIC 2 and NIC 3 is 345

  /*
  // AES-NI key setup
  unsigned char key[] = "0123456789abcdef";
  rk.roundkey = aes_assembly_init(key);
  rk.iv = (unsigned char*)malloc(16 * sizeof(char));
  */
  return 0;
}


int XIANEWXIDRouteTable::sync_interface(void) {
  // not implemented
  return 0;
}

// send a packet to neighbor AD router
int XIANEWXIDRouteTable::send_egress(Packet *m, uint8_t dpdk_rx_port) {
  struct ipv4_hdr *ipv4_hdr;
  /*
  // struct udp_hdr *udp_hdr;
  ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
      struct ether_hdr));
  // udp_hdr = (struct udp_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
  //                                 struct ether_hdr) +
  //                             sizeof(struct ipv4_hdr));
  
  // Specify output dpdk port.
  // Update destination IP address and UDP port number

  ipv4_hdr->dst_addr = neighbor_ad_router_ip[dpdk_rx_port];
  // udp_hdr->dst_port = SCION_UDP_PORT;

  // TODO update IP checksum
  // TODO should we updete destination MAC address?

  // TODO update destination MAC address
  l2fwd_send_packet(m, port_map[dpdk_rx_port].egress);
  */
  return 0;
}


// send a packet to the edge router that has next_ifid in this AD
int XIANEWXIDRouteTable::send_ingress(Packet *m, uint32_t next_ifid,
                               uint8_t dpdk_rx_port) {
  struct ipv4_hdr *ipv4_hdr;

  /*
  // struct udp_hdr *udp_hdr;
  ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
      struct ether_hdr));
  // udp_hdr = (struct udp_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
  //                                 struct ether_hdr) +
  //                             sizeof(struct ipv4_hdr));

  if (next_ifid != 0) {
    // Specify output dpdk port.
    // Update destination IP address and UDP port number
    ipv4_hdr->dst_addr = GET_EDGE_ROUTER_IPADDR(next_ifid);
    // udp_hdr->dst_port = SCION_UDP_PORT;

    // TODO update IP checksum
    // TODO should we updete destination MAC address?

    RTE_LOG(DEBUG, HSR, "egress dpdk_port=%d\n", DPDK_LOCAL_PORT);
    // TODO update destination MAC address
    l2fwd_send_packet(m, port_map[dpdk_rx_port].local);
    return 1;
  }
  */
  return -1;
}


uint8_t XIANEWXIDRouteTable::get_type(SCIONHeader *hdr) {
  SCIONAddr *src = (SCIONAddr *)(&hdr->srcAddr);
  //if (src->host_addr[0] != 10)
  //  return DATA_PACKET;
  //if (src->host_addr[1] != 224)
  //  return DATA_PACKET;
  //if (src->host_addr[2] != 0)
  //  return DATA_PACKET;

  SCIONAddr *dst = (SCIONAddr *)(&hdr->dstAddr);
  if (dst->host_addr[0] != 10)
    return DATA_PACKET;
  if (dst->host_addr[1] != 224)
    return DATA_PACKET;
  if (dst->host_addr[2] != 0)
    return DATA_PACKET;

  int b1 = src->host_addr[3] == BEACON_PACKET ||
           src->host_addr[3] == PATH_MGMT_PACKET ||
           src->host_addr[3] == CERT_CHAIN_REP_PACKET ||
           src->host_addr[3] == TRC_REP_PACKET;
  int b2 = dst->host_addr[3] == PATH_MGMT_PACKET ||
           dst->host_addr[3] == TRC_REQ_PACKET ||
           dst->host_addr[3] == TRC_REQ_LOCAL_PACKET ||
           dst->host_addr[3] == CERT_CHAIN_REQ_PACKET ||
           dst->host_addr[3] == CERT_CHAIN_REQ_LOCAL_PACKET ||
           dst->host_addr[3] == IFID_PKT_PACKET;


  if (b1)
    return src->host_addr[3];
  else if (b2)
    return dst->host_addr[3];
  else
    return DATA_PACKET;
  // change to to 0 -- invalid type
  return 0;
  //return (uint8_t)&hdr->srcAddr;
}


// TODO Optimization
static inline uint8_t is_on_up_path(InfoOpaqueField *currOF) {
  if ((currOF->info & 0x1) ==
      1) { // low bit of type field is used for uppath/downpath flag
    return 1;
  }
  return 0;
}
// TODO Optimization
static inline uint8_t is_last_path_of(SCIONCommonHeader *sch) {
  uint8_t offset = SCION_COMMON_HEADER_LEN + sizeof(HopOpaqueField);
  return sch->currentOF == offset + sch->headerLen;
}
// TODO Optimization
static inline uint8_t is_regular(HopOpaqueField *currOF) {
  if ((currOF->info & (1 << 6)) == 0) {
    return 0;
  }
  return 1;
}

// TODO Optimization
static inline uint8_t is_continue(HopOpaqueField *currOF) {
  if ((currOF->info & (1 << 5)) == 0) {
    return 0;
  }
  return 1;
}
static inline uint8_t is_xovr(HopOpaqueField *currOF) {
  if ((currOF->info & (1 << 4)) == 0) {
    return 0;
  }
  return 1;
}

void XIANEWXIDRouteTable::process_ifid_request(Packet *m, uint8_t dpdk_rx_port) {
  // struct ether_hdr *eth_hdr;
  struct ipv4_hdr *ipv4_hdr;
  // struct udp_hdr *udp_hdr;
  IFIDHeader *ifid_hdr;
  /*
  RTE_LOG(DEBUG, HSR, "process ifid request\n");

  ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
      struct ether_hdr));
  // udp_hdr = (struct udp_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
  //                                 struct ether_hdr) +
  //                             sizeof(struct ipv4_hdr));
  ifid_hdr = (IFIDHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                struct ether_hdr) +
                            sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  ifid_hdr->reply_id = my_ifid[dpdk_rx_port]; // complete with current interface
                                           // (self.interface.if_id)

  int i;
  for (i = 0; i < MAX_NUM_BEACON_SERVERS; i++) {
    /* ipv4_hdr is defined in rte_ip.h
    ipv4_hdr->dst_addr = beacon_servers[i];
    */
    // TODO update IP checksum
    // udp_hdr->dst_port = SCION_UDP_PORT;
    // TODO update destination MAC address
    l2fwd_send_packet(m, port_map[dpdk_rx_port].local);
  }
}

void XIANEWXIDRouteTable::process_pcb(Packet *m, uint8_t from_bs,
                               uint8_t dpdk_rx_port) {

  /*
  struct ether_hdr *eth_hdr;
  struct ipv4_hdr *ipv4_hdr;
  struct udp_hdr *udp_hdr;
  */
  PathConstructionBeacon *pcb;
  /*
  RTE_LOG(DEBUG, HSR, "process pcb\n");

  ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
      struct ether_hdr));
  udp_hdr = (struct udp_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                   struct ether_hdr) +
                               sizeof(struct ipv4_hdr));
  pcb = (PathConstructionBeacon *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                       struct ether_hdr) +
                                   sizeof(struct ipv4_hdr) +
                                   sizeof(struct udp_hdr));
  */

  if (from_bs) { // from local beacon server to neighbor router
    uint8_t last_pcbm_index = sizeof(pcb->payload.ads) / sizeof(ADMarking) - 1;
    HopOpaqueField *last_hof = &(pcb->payload).ads[last_pcbm_index].pcbm.hof;
    /*
    if (my_ifid != (uint32_t)EGRESS_IF(last_hof)) {
      // Wrong interface set by BS.
      return;
    }
    */
    /*
    ipv4_hdr->dst_addr = neighbor_ad_router_ip[dpdk_rx_port];
    */
    // udp_hdr->dst_port = SCION_UDP_PORT; // neighbor router port

    // TODO update IP checksum
    // l2fwd_send_packet(m, DPDK_EGRESS_PORT);
    l2fwd_send_packet(m, port_map[dpdk_rx_port].egress);

  } else { // from neighbor router to local beacon server
    pcb->payload.if_id = my_ifid[dpdk_rx_port];
    /*
    ipv4_hdr->dst_addr = beacon_servers[0];
    */
    // udp_hdr->dst_port = SCION_UDP_PORT;

    // TODO update destination MAC address
    l2fwd_send_packet(m, port_map[dpdk_rx_port].local);
  }
}


void XIANEWXIDRouteTable::relay_cert_server_packet(Packet *m,
                                            uint8_t from_local_socket,
                                            uint8_t dpdk_rx_port) {
  /*
  struct ipv4_hdr *ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(
      m, unsigned char *)+sizeof(struct ether_hdr));

  if (from_local_socket) {
    ipv4_hdr->dst_addr = neighbor_ad_router_ip[dpdk_rx_port];
    // TODO update IP checksum
    // l2fwd_send_packet(m, DPDK_EGRESS_PORT, dpdk_rx_port);
    l2fwd_send_packet(m, port_map[dpdk_rx_port].egress);
  } else {
    ipv4_hdr->dst_addr = certificate_servers[0];
    // TODO update IP checksum
    // TODO update destination MAC address
    l2fwd_send_packet(m, port_map[dpdk_rx_port].local);
  }
  */
}

void XIANEWXIDRouteTable::process_path_mgmt_packet(Packet *m,
                                            uint8_t from_local_ad,
                                            uint8_t dpdk_rx_port) {
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  InfoOpaqueField *iof;

  /*
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN); // currentOF is an offset
                                                     // from
                                                     // common header
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN); // currentOF is an offset
                                                      // from
                                                      // common header

  if (from_local_ad == 0 && is_last_path_of(sch)) {
    deliver(m, PATH_MGMT_PACKET, dpdk_rx_port);
  } else {
    forward_packet(m, from_local_ad, dpdk_rx_port);
  }
}


void XIANEWXIDRouteTable::deliver(Packet *m, uint32_t ptype,
                           uint8_t dpdk_rx_port) {
  /*
  struct ipv4_hdr *ipv4_hdr;
  struct udp_hdr *udp_hdr;
  SCIONHeader *scion_hdr;
  
  RTE_LOG(DEBUG, HSR, "deliver\n");
  ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
      struct ether_hdr));
  udp_hdr = (struct udp_hdr *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                   struct ether_hdr) +
                               sizeof(struct ipv4_hdr));

  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));

  // TODO support IPv6
  if (ptype == PATH_MGMT_PACKET) {
    ipv4_hdr->dst_addr = path_servers[0];
    udp_hdr->dst_port = SCION_UDP_PORT;
  } else {
    // update destination IP address to the end hostadress
    rte_memcpy((void *)&ipv4_hdr->dst_addr,
               (void *)&scion_hdr->dstAddr + SCION_ISD_AD_LEN,
               SCION_HOST_ADDR_LEN);

    udp_hdr->dst_port = SCION_UDP_EH_DATA_PORT;
  }
  */
  // TODO update destination MAC address
  l2fwd_send_packet(m, port_map[dpdk_rx_port].local);
  return;
}

void XIANEWXIDRouteTable::forward_packet(Packet *m, uint32_t from_local_ad,
                                  uint8_t dpdk_rx_port) {

  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  InfoOpaqueField *iof;
  /*
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);

  if (from_local_ad == 0) {
    // Ingress entry point
    if (hof->info == OFT_XOVR_POINT) {
      handle_ingress_xovr(m, dpdk_rx_port);
    } else {
      ingress_normal_forward(m, dpdk_rx_port);
    }
  } else {
    // Egress entry point
    if (hof->info == OFT_XOVR_POINT) {
      handle_egress_xovr(m, dpdk_rx_port);
    } else {
      egress_normal_forward(m, dpdk_rx_port);
    }
  }
  return;
}

void XIANEWXIDRouteTable::ingress_shortcut_xovr(Packet *m,
                                         uint8_t dpdk_rx_port) {
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;
  /*
  RTE_LOG(DEBUG, HSR, "ingress shortcut xovr\n");
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);
  
  prev_hof = hof + 1;
  if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
    return;
  }

  // switch to next segment
  sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField) * 2;
  sch->currentOF += sizeof(HopOpaqueField) * 4;

  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  if (INGRESS_IF(hof) == 0 && is_last_path_of(sch)) {
    prev_hof = hof - 1;
    if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
      return;
    }

    deliver(m, DATA_PACKET, dpdk_rx_port);
  } else {
    send_ingress(m, EGRESS_IF(hof), dpdk_rx_port);
  }
  return;
}

void XIANEWXIDRouteTable::ingress_peer_xovr(Packet *m, uint8_t dpdk_rx_port) {
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  /*
  RTE_LOG(DEBUG, HSR, "ingress peer xovr\n");
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */

  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  uint16_t fwd_if;
  if (is_on_up_path(iof)) {
    prev_hof = hof + 2;
    fwd_if = INGRESS_IF(hof + 1);
  } else {
    prev_hof = hof + 1;
    fwd_if = EGRESS_IF(hof + 1);
  }

  if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
    return;
  }

  sch->currentOF += sizeof(HopOpaqueField);

  if (is_last_path_of(sch))
    deliver(m, DATA_PACKET, dpdk_rx_port);
  else
    send_ingress(m, fwd_if, dpdk_rx_port);
}

void XIANEWXIDRouteTable::ingress_core_xovr(Packet *m, uint8_t dpdk_rx_port) {
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;
  /*
  RTE_LOG(DEBUG, HSR, "ingress peer xovr\n");
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  uint32_t fwd_if;
  if (is_on_up_path(iof)) {
    prev_hof = NULL;
  } else {
    prev_hof = hof - 1;
  }

  if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
    return;
  }

  if (is_last_path_of(sch))
    deliver(m, DATA_PACKET, dpdk_rx_port);
  else {
    // Switch to next path segment
    sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField);
    sch->currentOF += sizeof(HopOpaqueField) * 2;
    iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                              SCION_COMMON_HEADER_LEN);
    hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                             SCION_COMMON_HEADER_LEN);

    if (is_on_up_path(iof)) {
      send_ingress(m, INGRESS_IF(hof), dpdk_rx_port);
    } else {
      send_ingress(m, EGRESS_IF(hof), dpdk_rx_port);
    }
  }
}


void XIANEWXIDRouteTable::handle_ingress_xovr(Packet *m, uint8_t dpdk_rx_port) {
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  InfoOpaqueField *iof;
  /*
  RTE_LOG(DEBUG, HSR, "handle ingresst xovr\n");
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */

  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  if (iof->info == OFT_SHORTCUT) {
    ingress_shortcut_xovr(m, dpdk_rx_port);
  } else if (iof->info == OFT_INTRA_ISD_PEER ||
             iof->info == OFT_INTER_ISD_PEER) {
    ingress_peer_xovr(m, dpdk_rx_port);
  } else if (iof->info == OFT_CORE) {
    ingress_core_xovr(m, dpdk_rx_port);
  } else {
    // invalid OF
  }
}


uint8_t XIANEWXIDRouteTable::verify_of(HopOpaqueField *hof, HopOpaqueField *prev_hof,
                                       uint32_t ts) {

  return 1;
  /*
#ifndef VERIFY_OF
  return 1;
#endif

#define MAC_LEN 3

  unsigned char input[16];
  unsigned char mac[16];

  //RTE_LOG(DEBUG, HSR, "verify_of\n");

  // setup input vector
  // rte_mov32 ((void*)input, (void *)hof+1); //copy exp_type and
  // ingress/egress IF (4bytes)
  // rte_mov64 ((void*)input+4, (void *)prev_hof+1); //copy previous OF except
  // info field (7bytes)
  // rte_mov32 ((void*)input+11, (void*)&ts);

  rte_memcpy((void *)input, (void *)hof + 1,
             4); // copy exp_type and  ingress/egress IF (4bytes)
  rte_memcpy((void *)input + 4, (void *)prev_hof + 1,
             7); // copy previous OF except info field (7bytes)
  rte_memcpy((void *)input + 11, (void *)&ts, 4);

  // pkcs7_padding
  input[15] = 1;

  // call AES-NI
  // int i;
  // for (i = 0; i < 16; i++)
  //  printf("%02x", input[i]);
  // printf("\n");
  CBCMAC1BLK(rk.roundkey, rk.iv, input, mac);
  // for (i = 0; i < 16; i++)
  //  printf("%02x", mac[i]);
  // printf("\n");

  if (memcmp((void *)hof + 5, &mac,
             MAC_LEN)) { // (void *)hof + 5 is address of mac.
    return 1;
  } else {
    RTE_LOG(WARNING, HSR, "invalid MAC\n");
    // return 0;
    // TODO DEBUG, currently disable MAC check
    return 1;
  }
  */
}


void XIANEWXIDRouteTable::ingress_normal_forward(Packet *m,
                                          uint8_t dpdk_rx_port) {
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;

  //RTE_LOG(DEBUG, HSR, "ingress normal forward\n");
  /*
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  // printf("Ingress %d, Egress %d\n", INGRESS_IF(hof), EGRESS_IF(hof));
  uint16_t next_ifid;
  if (is_on_up_path(iof)) {
    next_ifid = INGRESS_IF(hof);
    prev_hof = hof + 1;
  } else {
    next_ifid = EGRESS_IF(hof);
    prev_hof = hof - 1;
  }

  if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
    return;
  }

  if (next_ifid == 0 && is_last_path_of(sch)) {
    deliver(m, DATA_PACKET, dpdk_rx_port);
  } else {
    send_ingress(m, next_ifid, dpdk_rx_port);
  }
}


void XIANEWXIDRouteTable::handle_egress_xovr(Packet *m, uint8_t dpdk_rx_port) {
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  InfoOpaqueField *iof;
  /*
  RTE_LOG(DEBUG, HSR, "handle egress xovr\n");
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  if (iof->info == OFT_SHORTCUT) {
    egress_shortcut_xovr(m, dpdk_rx_port);
  } else if (iof->info == OFT_INTRA_ISD_PEER ||
             iof->info == OFT_INTER_ISD_PEER) {
    egress_peer_xovr(m, dpdk_rx_port);
  } else if (iof->info == OFT_CORE) {
    egress_core_xovr(m, dpdk_rx_port);
  } else {
    // invalid OF
  }
}


void XIANEWXIDRouteTable::egress_shortcut_xovr(Packet *m, uint8_t dpdk_rx_port) {
  egress_normal_forward(m, dpdk_rx_port);
}


void XIANEWXIDRouteTable::egress_peer_xovr(Packet *m, uint8_t dpdk_rx_port) {
  struct ether_hdr *eth_hdr;
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;
  /*
  RTE_LOG(DEBUG, HSR, "egress normal forward\n");
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  if (is_on_up_path(iof)) {
    prev_hof = hof - 1;
    if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
      return;
    }

    // Switch to next segment
    sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField) * 2;
    sch->currentOF += sizeof(HopOpaqueField) * 4;
  } else {

    prev_hof = hof - 2;
    if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
      return;
    }
    sch->currentOF += sizeof(HopOpaqueField);
  }

  send_egress(m, dpdk_rx_port);
}

void XIANEWXIDRouteTable::egress_core_xovr(Packet *m, uint8_t dpdk_rx_port) {
  struct ether_hdr *eth_hdr;
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;
  /*
  RTE_LOG(DEBUG, HSR, "egress core xovr\n");
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  if (is_on_up_path(iof)) {
    prev_hof = NULL;
  } else {
    prev_hof = hof + 1;
  }

  if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
    return;
  }

  sch->currentOF += sizeof(HopOpaqueField);
  send_egress(m, dpdk_rx_port);
}


void XIANEWXIDRouteTable::egress_normal_forward(Packet *m,
                                         uint8_t dpdk_rx_port) {
  struct ether_hdr *eth_hdr;
  SCIONHeader *scion_hdr;
  SCIONCommonHeader *sch;
  HopOpaqueField *hof;
  HopOpaqueField *prev_hof;
  InfoOpaqueField *iof;
  /*
  RTE_LOG(DEBUG, HSR, "egress normal forward\n");
  scion_hdr = (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                                  struct ether_hdr) +
                              sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
  */
  sch = &(scion_hdr->commonHeader);
  hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF +
                           SCION_COMMON_HEADER_LEN);
  iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF +
                            SCION_COMMON_HEADER_LEN);

  // printf("Ingress %d, Egress %d\n", INGRESS_IF(hof), EGRESS_IF(hof));
  if (is_on_up_path(iof)) {
    prev_hof = hof + 1;
  } else {
    prev_hof = hof - 1;
  }

  
  // TODO  verify MAC
  if (verify_of(hof, prev_hof, iof->timestamp) == 0) {
    return;
  }
  
  sch + sch->currentOF + sizeof(HopOpaqueField);

  // send packet to neighbor AD's router
  send_egress(m, dpdk_rx_port);
}


void XIANEWXIDRouteTable::handle_request(Packet *m, uint8_t dpdk_rx_port) {
  struct ether_hdr *eth_hdr;
  SCIONHeader *scion_hdr;

	printf("scion: handle request\n");

  /*
  RTE_LOG(DEBUG, HSR, "packet recieved, dpdk_port=%d\n", dpdk_rx_port);

  eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
  */
  // if (m->ol_flags & PKT_RX_IPV4_HDR )
  //if (m->ol_flags & PKT_RX_IPV4_HDR || eth_hdr->ether_type == ntohs(0x0800)) {
  if (1) {

    // from local socket?
    uint8_t from_local_socket = 0;
    if (dpdk_rx_port % 2 == 1) {
      from_local_socket = 1;
    }
    /*
    scion_hdr =
        (SCIONHeader *)(rte_pktmbuf_mtod(m, unsigned char *)+sizeof(
                            struct ether_hdr) +
                        sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
    */
    uint8_t ptype = get_type(scion_hdr);
    switch (ptype) {
    case DATA_PACKET:
      forward_packet(m, from_local_socket, dpdk_rx_port);
      break;
    case IFID_PKT_PACKET:
      if (!from_local_socket)
        process_ifid_request(m, dpdk_rx_port);
      //else
      //  RTE_LOG(WARNING, HSR, "IFID packet from local socket\n");

      break;
    case BEACON_PACKET:
      process_pcb(m, from_local_socket, dpdk_rx_port);
      break;
    case CERT_CHAIN_REQ_PACKET:
    case CERT_CHAIN_REP_PACKET:
    case TRC_REQ_PACKET:
    case TRC_REP_PACKET:
      relay_cert_server_packet(m, from_local_socket, dpdk_rx_port);
      break;
    case PATH_MGMT_PACKET:
      process_path_mgmt_packet(m, from_local_socket, dpdk_rx_port);
      break;
    default:
      //RTE_LOG(DEBUG, HSR, "unknown packet type %d ?\n", ptype);
      break;
    }
  }
}

void XIANEWXIDRouteTable::l2fwd_send_packet(Packet *m, uint8_t port){
  return;
}

// AES lib

//
CLICK_ENDDECLS
EXPORT_ELEMENT(XIANEWXIDRouteTable)
ELEMENT_MT_SAFE(XIANEWXIDRouteTable)
