#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <click/packet_anno.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include "click/dagaddr.hpp"
#include "xiaoverlayrouted.hh"
#include <click/router.hh>
#include <click/element.hh>
#include <click/hashtable.hh>
#include <click/handlercall.hh>


#define XID_SIZE	CLICK_XIA_XID_ID_LEN

#define SID_XOVERLAY "SID:1110000000000000000000000000000000001111"

CLICK_DECLS


#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sstream>
#include <algorithm>
#include <ctype.h>
#include <iostream>
#include <unistd.h>

#define INCLUDE_TEST_CODE 0


#define check_init() do { if (!_init) return init_err; } while (false);


// void XIARouter::close()
// {
// 	if (_connected)
// 		_cserr = _cs.close();
// 	_connected = false;
// }

// int XIARouter::version(std::string &ver)
// {
// 	if (!connected())
// 		return XR_NOT_CONNECTED;

// 	if ((_cserr = _cs.get_router_version(ver)) == 0)
// 		return XR_OK;
// 	return  XR_CLICK_ERROR;
// }

// int XIARouter::listRouters(std::vector<std::string> &rlist)
// {
// 	vector<string> elements;
// 	size_t n;

// 	if (!connected())
// 		return XR_NOT_CONNECTED;

// 	if ((_cserr = _cs.get_config_el_names(elements)) != 0)
// 		return XR_CLICK_ERROR;

// 	vector<string>::iterator it;
// 	for (it = elements.begin(); it < elements.end(); it++) {

// 		// cheap way of finding host and router devices, they both have a /xrc element
// 		if ((n = (*it).find("/xrc")) != string::npos) {
// 			rlist.push_back((*it).substr(0, n));
// 		}
// 	}
// 	return 0;
// }

// int XIARouter::getNeighbors(std::string xidtype, std::vector<std::string> &neighbors)
// {
// 	if (!connected()) {
// 		return XR_NOT_CONNECTED;
//   }
	
// 	std::string table = _router + "/xrc/n/proc/rt_" + xidtype;

// 	std::string neighborStr;
// 	if ((_cserr = _cs.read(table, "neighbor", neighborStr)) != 0) {
//     printf("couldn't read\n");
// 		return XR_CLICK_ERROR;
//   }

// 	std::string::size_type beg = 0;
// 	for (auto end = 0; (end = neighborStr.find(',', end)) != std::string::npos; ++end)
// 	{
// 		neighbors.push_back(neighborStr.substr(beg, end - beg));
// 		beg = end + 1;
// 	}
// 	return 0;
// }

// get the current set of route entries, return value is number of entries returned or < 0 on err
// int XIARouter::getRoutes(std::string xidtype, std::vector<XIARouteEntry> &xrt)
// {
//   std::string result;
//   vector<string> lines;
//   int n = 0;

//   if (!connected())
//     return XR_NOT_CONNECTED;

//   if (xidtype.length() == 0)
//     return XR_INVALID_XID;

//   if (getRouter().length() == 0)
//     return  XR_ROUTER_NOT_SET;

//   std::string table = _router + "/xrc/n/proc/rt_" + xidtype;

//   if ((_cserr = _cs.read(table, "list", result)) != 0)
//     return XR_CLICK_ERROR;

//   unsigned start = 0;
//   unsigned current = 0;
//   unsigned len = result.length();
//   string line;

//   xrt.clear();
//   while (current < len) {
//     start = current;
//     while (current < len && result[current] != '\n') {
//       current++;
//     }

//     if (start < current || current < len) {
//       line = result.substr(start, current - start);

//       XIARouteEntry entry;
//       unsigned start, next;
//       string s;
//       int port;

//       start = 0;
//       next = line.find(",");
//       entry.xid = line.substr(start, next - start);

//       start = next + 1;
//       next = line.find(",", start);
//       s = line.substr(start, next - start);
//       port = atoi(s.c_str());
//       entry.port = port;

//       start = next + 1;
//       next = line.find(",", start);
//       entry.nextHop = line.substr(start, next - start);

//       start = next + 1;
//       s = line.substr(start, line.length() - start);
//       entry.flags = atoi(s.c_str());

//       xrt.push_back(entry);
//       n++;
//     }
//     current++;
//   }

//   return n;
// }

// std::string XIARouter::itoa(signed i)
// {
//   std::string s;
//   std::stringstream ss;

//   ss << i;
//   s = ss.str();
//   return s;
// }

// int XIARouter::updateRoute(string cmd, const std::string &xid, int port, const std::string &next, unsigned long flags)
// {
//   string xidtype;
//   string mutableXID(xid);
//   size_t n;

//   if (!connected())
//     return XR_NOT_CONNECTED;

//   if (mutableXID.length() == 0)
//     return XR_INVALID_XID;

//   if (next.length() > 0 && next.find(":") == string::npos)
//     return XR_INVALID_XID;

//   n = mutableXID.find(":");
//   if (n == string::npos || n >= sizeof(xidtype))
//     return XR_INVALID_XID;

//   if (getRouter().length() == 0)
//     return  XR_ROUTER_NOT_SET;

//   xidtype = mutableXID.substr(0, n);

//   std::string table = _router + "/xrc/n/proc/rt_" + xidtype;
  
//   string default_xid("-"); 
//   if (mutableXID.compare(n+1, 1, default_xid) == 0)
//     mutableXID = default_xid;
    
//   std::string entry;

//   // remove command only takes an xid
//   if (cmd == "remove") 
//     entry = mutableXID;
//   else
//     entry = mutableXID + "," + itoa(port) + "," + next + "," + itoa(flags);

//   if ((_cserr = _cs.write(table, cmd, entry)) != 0)
//     return XR_CLICK_ERROR;
  
//   return XR_OK;
// }

// int XIARouter::addRoute(const std::string &xid, int port, const std::string &next, unsigned long flags)
// {
//   return updateRoute("add4", xid, port, next, flags);
// }

// int XIARouter::setRoute(const std::string &xid, int port, const std::string &next, unsigned long flags)
// {
//   return updateRoute("set4", xid, port, next, flags);
// }

// int XIARouter::delRoute(const std::string &xid)
// {
//   string next = "";
//   return updateRoute("remove", xid, 0, next, 0);
// }

// const char *XIARouter::cserror()
// {
//   switch(_cserr) {
//     case ControlSocketClient::no_err:
//       return "no error";
//     case ControlSocketClient::sys_err:
//       return "O/S or networking error, check errno for more information";
//     case ControlSocketClient::init_err:
//       return "tried to perform operation on an unconfigured ControlSocketClient";
//     case ControlSocketClient::reinit_err:
//       return "tried to re-configure the client before close()ing it";
//     case ControlSocketClient::no_element:
//       return "specified element does not exist";
//     case ControlSocketClient::no_handler:
//       return "specified handler does not exist";
//     case ControlSocketClient::handler_no_perm:
//       return "router denied access to the specified handler";
//     case ControlSocketClient::handler_err:
//       return "handler returned an error";
//     case ControlSocketClient::handler_bad_format:
//       return "bad format in calling handler";
//     case ControlSocketClient::click_err:
//       return "unexpected response or error from the router";
//     case ControlSocketClient::too_short:
//       return "user buffer was too short";
//   }
//   return "unknown";
// }


void
XIAOverlayRouted::add_handlers()
{
  add_write_handler("neighbor", add_neighbor, 0);
}

int
XIAOverlayRouted::add_neighbor(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
  printf("Called add neighbor\n");
  XIAOverlayRouted *r = static_cast<XIAOverlayRouted *>(e);

  Vector<String> args;
  cp_argvec(conf, args);

  if (args.size() != 4)
    return errh->error("Invalid args: ", conf.c_str());

  int port = 0;
  String addr, sid, ad, hid;

  if (!cp_string(args[0], &addr))
    return errh->error("Invalid addr");

  if (!cp_string(args[1], &ad))
    return errh->error("Invalid AD");

  // if (!cp_string(args[2], &hid))
  //   return errh->error("Invalid HID");

  if (!cp_string(args[2], &sid))
    return errh->error("Invalid SID");

  if (!cp_integer(args[3], &port))
    return errh->error("Invalid port");
  

  printf("Adding NeighborEntry addr:%s AD:%s SID:%s port:%d\n", 
    addr.c_str(), ad.c_str(), sid.c_str(), port);

  NeighborEntry *entry = new NeighborEntry();
  entry->addr.append(addr.c_str());
  entry->AD.append(ad.c_str());
  entry->SID.append(sid.c_str());
  entry->port = port;

  //todo: check if the entry already exits
  r->route_state.neighborTable[entry->AD] = entry;

  return 0;
}

// int 
// XIAOverlayRouted::getNeighbors(std::vector<std::string> &neighbors)
// {
  
//   for(int i=0; i<route_state.neighborTable.size(); i++) {
//     neighbors.push_back()
//   }
//   return 0;
// }

XIAOverlayRouted::XIAOverlayRouted()
{
	
	FILE *f = fopen("etc/address.conf", "r");	
	if (!f) {
		printf("Failed to open resolv.conf \n");
		return;
	}
  char *hostname = (char *)malloc(32);
	char ad[100], hid[100], re[100];
	fscanf(f,"%s %s %s %s", hostname, re, ad, hid);
	fclose(f);

	strcpy(route_state.myAD, ad+1);
	strncpy(route_state.myHID, hid, XID_SIZE);


	route_state.num_neighbors = 0; // number of neighbor routers
	route_state.calc_dijstra_ticks = 0;

	route_state.flags = F_EDGE_ROUTER;

	route_state.dual_router_AD = "NULL";
  assert(hostname);
  // gethostname(hostname, 32);
  _hostname = String(hostname, strlen(hostname));
    printf("\n----XIAOverlayRouted: Started with ad: %s hid: %s hostname: %s----\n", route_state.myAD,
    route_state.myHID, _hostname.c_str());
  c = 0;
}

XIAOverlayRouted::~XIAOverlayRouted()
{
}

std::string
XIAOverlayRouted::sendLSA() {
  string message;

  Node n_ad(route_state.myAD);
  printf("XIAOverlayRouted: sending lsa with ad %s\n", n_ad.to_string().c_str());
  // Node n_hid(route_state.myHID);

  Xroute::XrouteMsg msg;
  Xroute::LSAMsg    *lsa  = msg.mutable_lsa();
  Xroute::Node      *node = lsa->mutable_node();
  Xroute::XID       *ad   = node->mutable_ad();
  // Xroute::XID       *hid  = node->mutable_hid();

  msg.set_type(Xroute::LSA_MSG);
  msg.set_version(Xroute::XROUTE_PROTO_VERSION);

  lsa->set_flags(route_state.flags);
  ad ->set_type(n_ad.type());
  ad ->set_id(n_ad.id(), XID_SIZE);
  // hid->set_type(n_hid.type());
  // hid->set_id(n_hid.id(), XID_SIZE);

  map<std::string, NeighborEntry *>::iterator it;
  for ( it=route_state.neighborTable.begin() ; it != route_state.neighborTable.end(); it++ ) {
    Node p_ad(it->second->AD);
    // Node p_hid(it->second.HID);

    node = lsa->add_peers();
    ad   = node->mutable_ad();
    // hid  = node->mutable_hid();

    ad ->set_type(p_ad.type());
    ad ->set_id(p_ad.id(), XID_SIZE);
    // hid->set_type(p_hid.type());
    // hid->set_id(p_hid.id(), XID_SIZE);
  }

  msg.SerializeToString(&message);

  return message;
}

std::string
XIAOverlayRouted::sendHello() 
{
	int buflen, rc;
	string message;
	Node n_ad(route_state.myAD);
	Node n_hid(route_state.myHID);
	Node n_sid(SID_XOVERLAY);

	Xroute::XrouteMsg msg;
	Xroute::HelloMsg *hello = msg.mutable_hello();
	Xroute::Node     *node  = hello->mutable_node();
	Xroute::XID      *ad    = node->mutable_ad();
	Xroute::XID      *hid   = node->mutable_hid();
	Xroute::XID      *sid   = node->mutable_sid();

	msg.set_type(Xroute::HELLO_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	hello->set_flags(route_state.flags);
	ad->set_type(n_ad.type());
	ad->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);
	sid->set_type(n_sid.type());
	sid->set_id(n_sid.id(), XID_SIZE);


	// printf("XIAOverlayRouted: %s\n", msg.DebugString().c_str());
	// printf("**** sending lsa with and num_neighbors %d \n", route_state.num_neighbors);

	msg.SerializeToString(&message);
	return message;
}

int
XIAOverlayRouted::processLSA(const Xroute::XrouteMsg &msg) {

  string neighborAD, neighborHID, myAD;
  string destAD, destHID;

  // fix me once we don't need to rebroadcast the lsa
  const Xroute::LSAMsg& lsa = msg.lsa();

  Xroute::XID a = lsa.node().ad();

  Node  ad(a.type(), a.id().c_str(), 0);

  destAD  = ad.to_string();
  printf("\n\nIn processLSA with destAD %s\n", destAD.c_str());
  // FIXME: this only allows for a single dual stack router in the network
  if (lsa.flags() & F_IP_GATEWAY) {
    route_state.dual_router_AD = destAD;
  }

  if (destAD.compare(route_state.myAD) == 0) {
    // skip if from me
    return 1;
  }

  printf("In processLSA\n");

  map<std::string, NodeStateEntry>::iterator it = route_state.networkTable.find(destAD);
  if(it != route_state.networkTable.end()) {
    // For now, delete this dest AD entry in networkTable
    // (... we will re-insert the updated entry shortly)
    route_state.networkTable.erase (it);
  }

  // don't bother if there's nothing there???
  if (lsa.peers_size() == 0) {
    return 1;
  }


  // 2. Update the network table
  NodeStateEntry entry;
  entry.dest = destAD;
  entry.num_neighbors = lsa.peers_size();

  for (int i = 0; i < lsa.peers_size(); i++) {

    Node a(lsa.peers(i).ad().type(),  lsa.peers(i).ad().id().c_str(), 0);
//    Node h(lsa.peers(i).hid().type(), lsa.peers(i).hid().id().c_str(), 0);

    neighborAD  = a.to_string();
//    neighborHID = h.to_string();

    // fill the neighbors into the corresponding networkTable entry
    entry.neighbor_list.push_back(neighborAD);
    printf("Adding neighbor entry %s\n", neighborAD.c_str());
  }

  route_state.networkTable[destAD] = entry;


  // printf("LSA received src=%s, num_neighbors=%d \n",
  //  (route_state.networkTable[destAD].dest).c_str(),
  //  route_state.networkTable[destAD].num_neighbors );


  // route_state.calc_dijstra_ticks++;

  // if (route_state.calc_dijstra_ticks == CALC_DIJKSTRA_INTERVAL) {
  //   // Calculate Shortest Path algorithm
  //   syslog(LOG_DEBUG, "Calcuating shortest paths\n");
  //   calcShortestPath();
  //   route_state.calc_dijstra_ticks = 0;

  //   // update Routing table (click routing table as well)
  //   updateClickRoutingTable();
  // }

  return 1;
}

void
XIAOverlayRouted::push(int port, Packet *p_in)
{
  struct click_ip *ip;
  struct click_udp *udp;
	
  size_t a = sizeof(*ip) + sizeof(*udp);
  printf("XIAOverlayRouted: received packet of %d\n", p_in->length());
  Xroute::XrouteMsg xmsg;
  if(p_in->length() > a) {

    ip = (struct click_ip *) p_in->data();
    udp = (struct click_udp *) (ip + 1);
    size_t mlen = p_in->length()-a;
    string cs(((const char *)(udp + 1)), mlen);
    if(!xmsg.ParseFromString(cs)) {
      printf("XIAOverlayRouted : could not understand packet\n");
    }
    else {
      if(xmsg.type() == Xroute::HELLO_MSG) {
        printf("Received hello message from %s\n",inet_ntoa(ip->ip_src));
      }
      else if(xmsg.type() == Xroute::LSA_MSG) {
        printf("Received LSA_MSG from %s\n", inet_ntoa(ip->ip_src));
        processLSA(xmsg);
          // 5. rebroadcast this LSA
      }
      else {
        printf("Unknown msg type \n");
      }
    }
  }

  // std::string msg =  sendHello();
  std::string msg = sendLSA();
  c++;


  size_t qsize = sizeof(*ip) + sizeof(*udp) + msg.length();
  WritablePacket *q = Packet::make(qsize);
  memset(q->data(), '0', q->length());
  ip = (struct click_ip *) q->data();
  udp = (struct click_udp *) (ip + 1);
  char *data = (char *)(udp+1);
  memcpy(data, msg.c_str(), msg.length());

  ip->ip_v = 4;
  ip->ip_hl = 5;
  ip->ip_tos = 0x10;
  ip->ip_len = htons(q->length());
  ip->ip_id = htons(0); // what is this used for exactly?
  ip->ip_off = htons(IP_DF);
  ip->ip_ttl = 255;
  ip->ip_p = IP_PROTO_UDP;
  ip->ip_sum = 0;
  // struct in_addr *saddr = (struct in_addr *)malloc(sizeof(struct in_addr));
  struct in_addr *daddr = (struct in_addr *)malloc(sizeof(struct in_addr));
  // assert(saddr);
  assert(daddr);
  // inet_aton("10.0.1.128", saddr);
  // ip->ip_src = *saddr;

  udp->uh_sport = htons(8772);
  udp->uh_dport = htons(8772);
  udp->uh_ulen = htons(msg.length());

  q->set_ip_header(ip, ip->ip_hl << 2);


  if(c>1) {
    output(port).push(q);
    return;    
  }

  printf("\n**********************\n getting neighbors c :%d\n", c);
    
  std::map<std::string, NeighborEntry *>::iterator it;
  for(it=route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++) {
    String dst(it->second->addr.c_str());

    printf("dst : %s\n", dst.c_str());
    inet_aton(dst.c_str(), daddr);
    ip->ip_dst = *daddr;
    q->set_dst_ip_anno(IPAddress(*daddr));
    SET_DST_PORT_ANNO(q, htons(8772));
    int port = it->second->port;
    printf("Port %d\n", port);

    printf("XIAOverlayRouted: Pushing packet len %d to %s\n", q->length(), inet_ntoa(*daddr));
    output(port).push(q);

  }

  printf("\n**********************\n");  

}


CLICK_ENDDECLS
EXPORT_ELEMENT(XIAOverlayRouted)
ELEMENT_MT_SAFE(XIAOverlayRouted)
ELEMENT_LIBS(-lprotobuf -L../../api/lib -ldagaddr)
