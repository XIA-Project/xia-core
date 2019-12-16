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

#include <click/router.hh>
#include <click/element.hh>
#include <click/handlercall.hh>


#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



#endif
CLICK_DECLS

XIAXIDRouteTable::XIAXIDRouteTable(): _drops(0)
{
	char *hostname = (char *)malloc(32);
	assert(hostname);
	gethostname(hostname, 32);
	_hostname = String(hostname, strlen(hostname));
}

XIAXIDRouteTable::~XIAXIDRouteTable()
{
	_rts.clear();
}

int
XIAXIDRouteTable::configure(Vector<String> & /*conf*/, ErrorHandler * /*errh*/)
{
    //printf("XIAXIDRouteTable: configuring %s\n", this->name().c_str());

	_principal_type_enabled = 1;

    _rtdata.port = -1;
    _rtdata.flags = 0;
    _rtdata.nexthop = NULL;

	return 0;
}

int
XIAXIDRouteTable::set_enabled(int e)
{
	_principal_type_enabled = e;
	return 0;
}

int XIAXIDRouteTable::get_enabled()
{
	return _principal_type_enabled;
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
	add_write_handler("enabled", write_handler, (void *)PRINCIPAL_TYPE_ENABLED);
	add_read_handler("enabled", read_handler, (void *)PRINCIPAL_TYPE_ENABLED);
	add_write_handler("addIP", add_ip_handler, 0);
	add_read_handler("listIP", list_ip_handler, 0);
}

String
XIAXIDRouteTable::read_handler(Element *e, void *thunk)
{
	// printf("******called %s \n", __FUNCTION__);
	XIAXIDRouteTable *t = (XIAXIDRouteTable *) e;
    switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return String(t->get_enabled());

		default:
			return "<error>";
    }
}

int
XIAXIDRouteTable::write_handler(const String &str, Element *e, void *thunk, ErrorHandler * /*errh*/)
{
	// printf("******called %s \n", __FUNCTION__);
	XIAXIDRouteTable *t = (XIAXIDRouteTable *) e;
    switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return t->set_enabled(atoi(str.c_str()));
		default:
			return -1;
    }
}

void
XIAXIDRouteTable::add_entry_to_tbl_str(Element *e, String& tbl, String xid,
		XIARouteData* xrd)
{
	// printf("******called %s \n", __FUNCTION__);
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
XIAXIDRouteTable::list_routes_handler(Element *e, void *thunk)
{
	// printf("******called %s \n", __FUNCTION__);
	XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);
	XIARouteData *xrd = &table->_rtdata;

	// get the default route
	String tbl;
	add_entry_to_tbl_str(e, tbl, "-", xrd);

	// get the rest
	for(auto& it : table->_rts) {
		String xid = it.first.unparse();
		xrd = it.second;
		add_entry_to_tbl_str(e, tbl, xid, xrd);
	}
	return tbl;
}

String
XIAXIDRouteTable::list_ip_handler(Element *e, void *)
{

	XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);
	String ntable;
    HashTable<String, XIAXIDAddr*>::iterator it;
    for(it = table->_nts.begin(); it != table->_nts.end(); it++) {
    	ntable += String(it->first + "," + it->second->AD->c_str() + "," 
    		+ it->second->SID->c_str() + ";");
    }

	return ntable;
}

int
XIAXIDRouteTable::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	// handle older style route entries
	// printf("******called %s \n", __FUNCTION__);
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
		// If nexthop is less than 40 chars, it is an IP:port
		if(nxthop.length() < CLICK_XIA_XID_ID_LEN*2) {
			printf("XIAXIDRouteTable: Calling udpnext nts %lld\n", (unsigned long)&table->_nts);
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
XIAXIDRouteTable::add_ip_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	printf("In add ip handler\n");
	XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);

	Vector<String> args;
	String xid_str, ipstr;

	cp_argvec(conf, args);

	if (args.size() != 2) {
		printf("1 %ld\n", args.size());
		return errh->error("Invalid route(need 2 entries): ", conf.c_str());
	}

	if (!cp_string(args[0], &ipstr)) {
		printf("3\n");
		return errh->error("Invalid ipaddr arg");
	}

	if (!cp_string(args[1], &xid_str)) {
		printf("2\n");
		return errh->error("Invalid xid arg");
	}

	HashTable<String, XIAXIDAddr *>::iterator it = table->_nts.find(ipstr);
	XIAXIDAddr *n;
	if(it == table->_nts.end()) {
		printf("XIAXIDNeighbor: %s does not exist in HT %d \n", 
			ipstr.c_str(), table->_nts.size());
		n = new XIAXIDAddr();
		table->_nts[ipstr] = n;
	}
	else {
		n = it->second;
	}

	std::string *s = new std::string(xid_str.c_str());

	if(xid_str.starts_with("AD")) {
		n->AD = s;
		printf("Added AD for %s :  %s\n", ipstr.c_str(), table->_nts[ipstr]->AD->c_str());
	}
	else if(xid_str.starts_with("HID"))
	{
		n->HID = s;		
		printf("Added HID for %s :  %s\n", ipstr.c_str(), table->_nts[ipstr]->HID->c_str());
	}
	else if(xid_str.starts_with("SID")) {
		n->SID = s;
		printf("Added SID for %s :  %s\n", ipstr.c_str(), table->_nts[ipstr]->SID->c_str());
	}

	return 0;
}

int
XIAXIDRouteTable::set_udpnext(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{

	XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);
	printf("******called %s table : %lld\n", __FUNCTION__, (unsigned long)table);
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

	String ename = table->_hostname +  String("/xrc/n/proc/rt_IP");
	Element *ne = e->router()->find(ename);
	if(ne) {
		printf("XIAXIDRoutetable: Calling write\n");
		int ret = HandlerCall::call_write(ne, "addIP", ipaddrstr + "," + xid_str);
		if (ret) 
			return errh->error("Failed to call write\n");
	}
	else {
		printf("Element %s not found \n", ename.c_str());
		return errh->error("Failed to find element : \n", ename.c_str());
	}


	if(args.size() > 3) {
		int flags;
		// XIARouteData *xrd = &table->_rtdata;
		if (!cp_integer(args[3], &flags))
			return errh->error("invalid flags: ", conf.c_str());

		printf("XIAXIDRouteTable: Going into the neighbor block \n");
		if(flags == NEIGHBOR)
		{
			Vector <Element *> ve = e->router()->elements();
			for(int i=0; i<ve.size(); i++) {
				printf("Element: %s\n", ve[i]->name().c_str());
			}
			
			String ename = table->_hostname +  String("/xrc/n/proc/rt_IP");
			Element *ne = e->router()->find(ename);
			if(ne) {
				String result = HandlerCall::call_read(ne, "listIP");
				std::string rstr(result.c_str (), result.length());

				std::string::size_type beg = 0;
				printf("Returned %s\n", rstr.c_str());
				String ad, sid;
				for (auto end = 0; (end = rstr.find(';', end)) != std::string::npos; end++)
				{
					std::string n(rstr.substr(beg, end - beg));
					printf("N : %s\n", n.c_str());
					Vector<String>args;
					String c(n.c_str());
					cp_argvec(c, args);
					if(args.size() != 3) {
						printf("List IP did not return all args %d\n", args.size());
						return errh->error("Not all args");
					}
					if(args[0].equals(ipaddrstr)) {
						cp_string(args[1], &ad);
						cp_string(args[2], &sid);
						break;
					}
					beg = end + 1;
				}
				String rname = "rd/rd";
				Element *re = e->router()->find(rname);
				if(re) {
					String nentry(ipaddrstr + "," + ad + "," + sid + "," + std::to_string(port).c_str());
					if(HandlerCall::call_write(re, "neighbor", nentry)) {
						printf("Failed to write to xrouted\n");
					}	
					else {
						printf("Written %s\n", nentry.c_str());
					}
				}


			}
			else {
				printf("Element %s not found \n", ename.c_str());
			}
			// XIAXIDNeighbor *neighbor = (XIAXIDNeighbor *)malloc(sizeof(XIAXIDNeighbor));
			// if(!neighbor) {
			// 	printf("XIAXIDRouteTable: allocation failed\n");
			// }
			// String *s = new String(ipaddrstr + ":" + portstr);
			// neighbor->addr = s;
			// neighbor->iface = port;
			// printf("XIAXIDRouteTable: Adding neighbor %s at port %d\n", (*(neighbor->addr)).c_str(), 
			// 	neighbor->iface);
			// table->_ntable.push_back(neighbor);
			// printf("XIAXIDRoutable: Pushed %s\n", (*(table->_ntable[0]->addr)).c_str());
			// // table->_ntable.count += 1;
			// printf("XIAXIDRouteTable: neighbor count %ld \n", table->_ntable.size());
			// 
			// 
			// printf("neighbor entry found %s %s %s %s\n", ipaddrstr.c_str(), 
			// 	table->_nts[ipaddrstr]->AD->c_str(), table->_nts[ipaddrstr]->HID->c_str(), table->_nts[ipaddrstr]->SID->c_str());
			
		}
	}

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
XIAXIDRouteTable::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
	// printf("******called %s \n", __FUNCTION__);
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
XIAXIDRouteTable::load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
	// printf("******called %s \n", __FUNCTION__);
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
	printf("loaded %d entries", c);

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
			printf("Error at %s %d\n", __FUNCTION__, __LINE__);
		break;
	}
	curpos+=(eol+1-buf);
		eol[1] = '\0';
		if (strlen(buf) == 0)
			continue;

		if (set_handler(buf, e, 0, errh) != 0) {
			printf("Error at %s %d\n", __FUNCTION__, __LINE__);
			return -1;
	}
		c++;
	}
	set_fs(old_fs);

	printf("XIA routing table loaded %d entries", c);
	return 0;
#endif
}

int
XIAXIDRouteTable::generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
	// printf("******called %s \n", __FUNCTION__);
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

	if (port<0) printf("Random %d ports", -port);

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
				printf("random value %x", *reinterpret_cast<uint32_t*>(xid));
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
				printf("Random port for XID %s #%d: %d ",XID(xid_d).unparse_pretty(e).c_str(), i, random);
		} else
			xrd->port = port;

		table->_rts[XID(xid_d)] = xrd;
	}

	printf("generated %d entries", count);
	return 0;
}


void
XIAXIDRouteTable::push(int /*in_ether_port*/, Packet *p)
{
	// printf("******called %s \n", __FUNCTION__);
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
	  output(1).push(p);
	}
	else {
      // no route, feed back into the route engine
	  output(2).push(p);
    }
}

// int
// XIAXIDRouteTable::process_xcmp_redirect(Packet *p)
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
XIAXIDRouteTable::lookup_route(Packet *p)
{
   // printf("******called %s \n", __FUNCTION__);
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
EXPORT_ELEMENT(XIAXIDRouteTable)
ELEMENT_MT_SAFE(XIAXIDRouteTable)
