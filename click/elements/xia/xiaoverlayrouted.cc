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
#include <click/standard/scheduleinfo.hh>
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


void
XIAOverlayRouted::add_handlers()
{
  add_write_handler("addNeighbor", add_neighbor, 0);
  add_write_handler("removeNeighbor", remove_neighbor, 0);

}

int
XIAOverlayRouted::remove_neighbor(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
  XIAOverlayRouted *r = static_cast<XIAOverlayRouted *>(e);

  Vector<String> args;
  cp_argvec(conf, args);

  if (args.size() != 1)
    return errh->error("Invalid args: ", conf.c_str());

  String ad;

  if (!cp_string(args[0], &ad)) {
    return errh->error("Invalid AD");
  }

  std::map<std::string, NeighborEntry*>::iterator it;
  std::string ad_str(ad.c_str());
  it = r->route_state.neighborTable.find(ad_str);
  if(it != r->route_state.neighborTable.end()) {
    NeighborEntry *n = it->second;
    r->route_state.neighborTable.erase(it);
    delete(n);
    printf("XIAOverlayRouted: removed neighborAD %s\n", ad.c_str());
  }
  else {
    return errh->error("AD not found ", ad.c_str());
  }

  return 0;
}

int
XIAOverlayRouted::add_neighbor(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
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

  r->_n_updates = 1;

  return 0;
}


XIAOverlayRouted::XIAOverlayRouted()
{
  
  FILE *f = fopen("etc/address.conf", "r"); 
  if (!f) {
    printf("Failed to open address.conf \n");
    return;
  }
  char *hostname = (char *)malloc(32);
  assert(hostname);
  char ad[100], hid[100], re[100];
  fscanf(f,"%s %s %s %s", hostname, re, ad, hid);
  fclose(f);

  strcpy(route_state.myAD, ad+1);
  strncpy(route_state.myHID, hid, XID_SIZE);

  route_state.num_neighbors = 0; // number of neighbor routers
  route_state.calc_dijstra_ticks = 0;

  route_state.flags = F_EDGE_ROUTER;

  route_state.dual_router_AD = "NULL";
  _hostname = String(hostname, strlen(hostname));
    printf("\n----XIAOverlayRouted: Started with ad: %s hid: %s hostname: %s----\n", route_state.myAD,
    route_state.myHID, _hostname.c_str());
  c = 0;
  _n_updates = 0;
}

XIAOverlayRouted::~XIAOverlayRouted()
{
}

std::string
XIAOverlayRouted::sendLSA() {
  string message;

  Node n_ad(route_state.myAD);
  // printf("XIAOverlayRouted: sending lsa with ad %s\n", n_ad.to_string().c_str());
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
  // printf("\n\nIn processLSA with destAD %s\n", destAD.c_str());
  // FIXME: this only allows for a single dual stack router in the network
  if (lsa.flags() & F_IP_GATEWAY) {
    route_state.dual_router_AD = destAD;
  }

  if (destAD.compare(route_state.myAD) == 0) {
    // skip if from me
    return 1;
  }

  // printf("In processLSA\n");

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
    // printf("Adding neighbor entry %s\n", neighborAD.c_str());
  }

  route_state.networkTable[destAD] = entry;


  // printf("LSA received src=%s, num_neighbors=%d \n",
   // (route_state.networkTable[destAD].dest).c_str(),
   // route_state.networkTable[destAD].num_neighbors );


  // route_state.calc_dijstra_ticks++;

  // if (route_state.calc_dijstra_ticks == CALC_DIJKSTRA_INTERVAL) {
    // Calculate Shortest Path algorithm
    // printf("Calcuating shortest paths\n");
    // calcShortestPath();
    // route_state.calc_dijstra_ticks = 0;

    // // update Routing table (click routing table as well)
    // updateClickRoutingTable();
    updateRTable();
  // }

  return 1;
}

void
XIAOverlayRouted::updateRTable() {
    // printf("Calcuating shortest paths\n");
    calcShortestPath();
    route_state.calc_dijstra_ticks = 0;

    // update Routing table (click routing table as well)
    updateClickRoutingTable();

    _n_updates = 0;
}

void 
XIAOverlayRouted::calcShortestPath() {

  // first, clear the current routing table
  route_state.ADrouteTable.clear();

  map<std::string, NodeStateEntry>::iterator it1;
  for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end();) {

    // filter out an abnormal case
    if(it1->second.num_neighbors == 0 || (it1->second.dest).empty() ) {
      route_state.networkTable.erase (it1++);
    } else {
      ++it1;
    }
  }


  // work on a copy of the table
  map<std::string, NodeStateEntry> table;
  table = route_state.networkTable;

  for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {
    // initialize the checking variable
    it1->second.checked = false;
    it1->second.cost = 10000000;
  }

  // compute shortest path
  // initialization
  string myAD, tempAD;
  myAD = route_state.myAD;
  route_state.networkTable[myAD].checked = true;
  route_state.networkTable[myAD].cost = 0;
  table.erase(myAD);


  std::map<std::string, NeighborEntry*>::iterator it2;
  // for ( it2=route_state.neig[myAD].neighbor_list.begin() ; it2 < route_state.networkTable[myAD].neighbor_list.end(); it2++ ) {

  for( it2 = route_state.neighborTable.begin(); it2 != route_state.neighborTable.end(); it2++) {
    tempAD = it2->first.c_str();
    route_state.networkTable[tempAD].cost = 1;
    route_state.networkTable[tempAD].prevNode = myAD;
  }

  // loop
  while (!table.empty()) {
    int minCost = 10000000;
    string selectedAD, tmpAD;
    for ( it1=table.begin() ; it1 != table.end(); it1++ ) {
      tmpAD = it1->second.dest;
      if (route_state.networkTable[tmpAD].cost < minCost) {
        minCost = route_state.networkTable[tmpAD].cost;
        selectedAD = tmpAD;
      }
    }
    if(selectedAD.empty()) {
      return;
    }

    table.erase(selectedAD);
    route_state.networkTable[selectedAD].checked = true;

    vector<std::string>::iterator it3;
    for ( it3=route_state.networkTable[selectedAD].neighbor_list.begin() ; it3 < route_state.networkTable[selectedAD].neighbor_list.end(); it3++ ) {
      tempAD = (*it3).c_str();    
      if (route_state.networkTable[tempAD].checked != true) {
        if (route_state.networkTable[tempAD].cost ==0 || route_state.networkTable[tempAD].cost > route_state.networkTable[selectedAD].cost + 1) {

          if(route_state.networkTable[tempAD].cost == 0) {
            route_state.networkTable[tempAD].dest = tempAD;
            route_state.networkTable[tempAD].num_neighbors = 0;
          }

          route_state.networkTable[tempAD].cost = route_state.networkTable[selectedAD].cost + 1;
          route_state.networkTable[tempAD].prevNode = selectedAD;

        }
      }
    }
  }

  std::string tempAD1, tempAD2;
  int hop_count;
  // set up the nexthop
  for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {
    tempAD1 = it1->second.dest;
    if ( myAD.compare(tempAD1) != 0 ) {
      tempAD2 = tempAD1;
      // tempAD2 = route_state.networkTable[tempAD2].prevNode;
      hop_count = 0;
      while (route_state.networkTable[tempAD2].prevNode.compare(myAD)!=0 && hop_count < MAX_HOP_COUNT) {
        tempAD2 = route_state.networkTable[tempAD2].prevNode;
        hop_count++;
      }
      if(hop_count < MAX_HOP_COUNT) {
        route_state.ADrouteTable[tempAD1].dest = tempAD1;
        // route_state.ADrouteTable[tempAD1].nextHop = route_state.neighborTable[tempAD2].HID;
        if(tempAD1.compare(tempAD2) != 0) {
          route_state.ADrouteTable[tempAD1].nextHop = route_state.neighborTable[tempAD2]->AD;
        }
        // add ipaddr as nexthop for neighbor
        else {
          route_state.ADrouteTable[tempAD1].nextHop = route_state.neighborTable[tempAD2]->addr + ":8770";
        }
        route_state.ADrouteTable[tempAD1].port = route_state.neighborTable[tempAD2]->port;
      }
    }
  }
  // printf("\n\n");
  // printRoutingTable();
}

void
XIAOverlayRouted::printRoutingTable() {

  printf("AD Routing table at %s", route_state.myAD);
  map<std::string, RouteEntry>::iterator it1;
  for ( it1=route_state.ADrouteTable.begin() ; it1 != route_state.ADrouteTable.end(); it1++ ) {
    printf("Dest=%s, NextHop=%s, Port=%d, Flags=%u", (it1->second.dest).c_str(), 
    (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags) );
  }
}


void 
XIAOverlayRouted::updateClickRoutingTable() {

  int rc, port;
  uint32_t flags;
  string destXID, nexthopXID;
  string default_AD("AD:-"), default_HID("HID:-"), default_4ID("IP:-");

  map<std::string, RouteEntry>::iterator it1;
  for ( it1=route_state.ADrouteTable.begin() ; it1 != route_state.ADrouteTable.end(); it1++ ) {
    destXID = it1->second.dest;
    nexthopXID = it1->second.nextHop;
    port =  it1->second.port;
    flags = it1->second.flags;

    if ((rc = updateRoute("set4", destXID, port, nexthopXID, flags)) != 0)
      printf("error setting route %d", rc);

    // set default AD for 4ID traffic
    if (!(route_state.flags & F_IP_GATEWAY) && destXID.compare(route_state.dual_router_AD) == 0) {
      if ((rc = updateRoute("set4", default_4ID, port, nexthopXID, flags)) != 0)
        printf("error setting route %d", rc);
    }
  }
  // listRoutes("AD");
  // listRoutes("HID");
}

String XIAOverlayRouted::itoa(signed i)
{
  std::string s;
  std::stringstream ss;

  ss << i;
  s = ss.str();
  return String(s.c_str());
}

int 
XIAOverlayRouted::updateRoute(string cmd, const std::string &xid, int port,
  const std::string &next, unsigned long flags)
{
  string xidtype; 
  string mutableXID(xid);
  size_t n;

  if (mutableXID.length() == 0)
    return 1;

  if (next.length() > 0 && next.find(":") == string::npos)
    return 1;

  n = mutableXID.find(":");
  if (n == string::npos || n >= sizeof(xidtype))
    return 1;

  xidtype = mutableXID.substr(0, n);


  std::string table = std::string(_hostname.c_str()) + "/xrc/n/proc/rt_" + xidtype;
  
  string default_xid("-"); 
  if (mutableXID.compare(n+1, 1, default_xid) == 0)
    mutableXID = default_xid;
    
  String entry;

  // remove command only takes an xid
  if (cmd == "remove") 
    entry = mutableXID.c_str();
  else {
    String sep(",");
    entry = String(mutableXID.c_str()) + sep + itoa(port) + sep + String(next.c_str()) + sep + itoa(flags);
  }

  // printf("\nXIAOverlayRouted: updateRoute for %s \n",entry.c_str());

  Element *re = this->router()->find(String(table.c_str()));
  if(re) {
    int ret = HandlerCall::call_write(re, cmd.c_str(), entry);
    if(ret)
    {
      printf("HandlerCall failed\n");
      return 2;
    }
  }
  else {
    printf("Element not found");
    return 2;
  }
  
  return 0;
}

void
XIAOverlayRouted::neighbor_broadcast(std::string msg) {
  std::map<std::string, NeighborEntry *>::iterator it;
    for(it=route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++) {
      String dst(it->second->addr.c_str());
      int port = it->second->port;
      _push_msg(msg, dst, port);
    }
}

void
XIAOverlayRouted::_push_msg(std::string msg, String dst, int port) {

  struct click_ip *ip;
  struct click_udp *udp;

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

  //todo: fill source address
  // struct in_addr *saddr = (struct in_addr *)malloc(sizeof(struct in_addr));
  struct in_addr *daddr = (struct in_addr *)malloc(sizeof(struct in_addr));
  // assert(saddr);
  assert(daddr);
  // inet_aton("10.0.1.128", saddr);
  // ip->ip_src = *saddr;

  //todo: make this configurable
  udp->uh_sport = htons(8772);
  udp->uh_dport = htons(8772);
  udp->uh_ulen = htons(msg.length());

  q->set_ip_header(ip, ip->ip_hl << 2);


  inet_aton(dst.c_str(), daddr);
  ip->ip_dst = *daddr;
  q->set_dst_ip_anno(IPAddress(*daddr));
  SET_DST_PORT_ANNO(q, htons(8772));
  // int port = it->second->port;
  printf("XIAOverlayRouted: Pushing packet len %d to %s, port :%d\n",
   q->length(), inet_ntoa(*daddr), port);
  output(port).push(q);

}

void
XIAOverlayRouted::push(int port, Packet *p_in)
{
  struct click_ip *ip;
  struct click_udp *udp;
  
  size_t a = sizeof(*ip) + sizeof(*udp);
  // printf("XIAOverlayRouted: received packet of %d\n", p_in->length());
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
        // printf("Received LSA_MSG from %s\n", inet_ntoa(ip->ip_src));
        processLSA(xmsg);
          // 5. rebroadcast this LSA
      }
      else {
        printf("Unknown msg type \n");
      }
    }
  }
  p_in->kill();
}

int
XIAOverlayRouted::initialize(ErrorHandler *errh) {
  _timer.initialize(this);
  _ticks = new Timer(this);
  _ticks->initialize(this);
  _ticks->schedule_after_msec(CALC_DIJKSTRA_INTERVAL);

  return 0;
}

void
XIAOverlayRouted::run_timer(Timer *timer) {

  // printf("XIAOverlayRouted: run_task\n");
  // route_state.calc_dijstra_ticks++;
  // if (route_state.calc_dijstra_ticks == CALC_DIJKSTRA_INTERVAL) {
  if(_n_updates)
    updateRTable();
  // }

  std::string msg = sendLSA();
  // printf("\n**********************\n getting neighbors c :%d\n", c);
  neighbor_broadcast(msg);
  // printf("\n**********************\n");
  _ticks->reschedule_after_sec(CALC_DIJKSTRA_INTERVAL);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(XIAOverlayRouted)
ELEMENT_MT_SAFE(XIAOverlayRouted)
ELEMENT_LIBS(-lprotobuf -L../../api/lib -ldagaddr)
