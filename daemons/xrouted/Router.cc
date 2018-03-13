/*
** Copyright 2017 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>

#include "../common/XIARouter.hh"
#include "Router.hh"
#include "dagaddr.hpp"
#include "minIni.h"

// FIXME:
// import sid routehandler?
// does the dual router flag serve any purpose?
// implement security!!!


// purge stale entries from our tables
void Router::purge()
{
	// initialize if this is first time in
	if (_last_route_purge == 0) {
		_last_route_purge = time(NULL);
		_last_neighbor_purge = time(NULL);
		return;
	}

	time_t now = time(NULL);

	// walk list of routes given to us by the controller
	//  and delete any that are stale
	if (now - _last_route_purge >= ROUTE_EXPIRE_TIME) {
		_last_route_purge = now;

		TimestampList::iterator iter = _route_timestamp.begin();
		while (iter != _route_timestamp.end()) {

			if (now - iter->second >= ROUTE_EXPIRE_TIME) {
				syslog(LOG_INFO, "purging route for : %s", iter->first.c_str());
				_xr.delRoute(iter->first);
				_route_timestamp.erase(iter++);

			} else {
				++iter;
			}
		}
	}

	// walk list of neighbors we've discovered via keepalive
	//  and delete any that are stale
	if (now - _last_neighbor_purge >= NEIGHBOR_EXPIRE_TIME) {
		_last_neighbor_purge = now;

		TimestampList::iterator iter = _neighbor_timestamp.begin();
		while (iter !=  _neighbor_timestamp.end()) {

		    time_t t = iter->second;
		    if ((t != 0) && (now - t >= NEIGHBOR_EXPIRE_TIME)) {
				syslog(LOG_INFO, "purging neighbor route for : %s", iter->first.c_str());
				_xr.delRoute(iter->first);
				_neighborTable.erase(iter->first);
				_neighbor_timestamp.erase(iter++);

			} else {
				++iter;
			}
		}
	}
}


int Router::handler()
{
	int rc;
	char recv_message[BUFFER_SIZE];
	struct pollfd pfds[3];
	int iface;
	bool local;

	bzero(pfds, sizeof(pfds));
	pfds[0].fd = _local_sock;
	pfds[1].fd = _broadcast_sock;
	pfds[2].fd = _recv_sock;
	pfds[0].events = POLLIN;
	pfds[1].events = POLLIN;
	pfds[2].events = POLLIN;

	// only poll for local sock if we haven't gotten config info from the network
	int num_pfds = _joined ? 3 : 1;

	// get the next incoming message
	if ((rc = readMessage(recv_message, pfds, num_pfds, &iface, &local)) > 0) {
		processMsg(string(recv_message, rc), iface, local);
	}

	if (_joined) {
		// once we are initialized, purge any stale routes
		//  and start sending keepalive's and lsa's
		purge();

		struct timeval now;
		gettimeofday(&now, NULL);

		if (timercmp(&now, &h_fire, >=)) {
			sendKeepalive();
			timeradd(&now, &h_freq, &h_fire);
		}
		if (timercmp(&now, &l_fire, >=)) {
			sendLSA();
			timeradd(&now, &l_freq, &l_fire);
		}
	}

	return 0;
}


// now that we've joined the network, set up our dags and sockets
int Router::makeSockets()
{
	char s[MAX_DAG_SIZE];
	Graph g;

	Node src;
	Node nHID(_myHID);
	Node nAD(_myAD);

	// broadcast socket - keepalive messages, can hopefully go away
	g = src * Node(broadcast_fid) * Node(intradomain_sid);
	if ((_broadcast_sock = makeSocket(g, &_broadcast_dag)) < 0) {
		return -1;
	}
	syslog(LOG_INFO, "Broadcast: %s", g.dag_string().c_str());

	// router socket - flooded & interdomain communication
	XcreateFID(s, sizeof(s));
	Node nFID(s);
	XmakeNewSID(s, sizeof(s));
	Node rSID(s);
	g = (src * nHID * nFID * rSID) + (src * nFID * rSID);

	if ((_recv_sock = makeSocket(g, &_recv_dag)) < 0) {
		return -1;
	}
	_recv_sid = std::string(s);
	syslog(LOG_INFO, "Flood: %s", g.dag_string().c_str());

	// source socket - sending socket
	XmakeNewSID(s, sizeof(s));
	Node outSID(s);
	g = src * nAD * nHID * outSID;

	if ((_source_sock = makeSocket(g, &_source_dag)) < 0) {
		return -1;
	}
	syslog(LOG_INFO, "Source: %s", g.dag_string().c_str());

	return 0;
}


int Router::init()
{
	_joined = false; // we're not part of a network yet

	// FIXME: figure out what the correct type of router we are
	_flags = F_EDGE_ROUTER;

	struct timeval now;

	h_freq.tv_sec = 0;
	h_freq.tv_usec = 100000;
	l_freq.tv_sec = 0;
	l_freq.tv_usec = 300000;

	_last_route_purge = 0;
	_last_neighbor_purge = 0;

	gettimeofday(&now, NULL);
	timeradd(&now, &h_freq, &h_fire);
	timeradd(&now, &l_freq, &l_fire);

	return 0;
}

// send keepalive on broadcast interface
// will eventually go away when netjoin does this for us
int Router::sendKeepalive()
{
	// FIXME: removed sending SIDs for now, do we need to in the future???
	string message;

	Node n_ad(_myAD);
	Node n_hid(_myHID);
	Node n_sid(intradomain_sid);

	Xroute::XrouteMsg msg;
	Xroute::KeepaliveMsg *ka = msg.mutable_keepalive();
	Xroute::Node     *node  = ka->mutable_node();
	Xroute::XID      *ad    = node->mutable_ad();
	Xroute::XID      *hid   = node->mutable_hid();
	//Xroute::XID      *sid   = node->mutable_sid();

	msg.set_type(Xroute::KEEPALIVE_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	ka->set_flags(_flags);
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);
	// sid->set_type(n_sid.type());
	// sid->set_id(n_sid.id(), XID_SIZE);

	return sendBroadcastMessage(msg);
}


// send LinkStateAdvertisement message
// messages will flood eventually, and will then become unicast
// once the network routes get set up
int Router::sendLSA()
{

	//syslog(LOG_DEBUG, "sending routing table");
	Node n_ad(_myAD);
	Node n_hid(_myHID);

	Xroute::XrouteMsg msg;
	Xroute::LSAMsg    *lsa  = msg.mutable_lsa();
	Xroute::Node      *node = lsa->mutable_node();
	Xroute::XID       *ad   = node->mutable_ad();
	Xroute::XID       *hid  = node->mutable_hid();

	msg.set_type(Xroute::LSA_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);

	lsa->set_flags(_flags);
	lsa->set_dag(&_recv_dag, sockaddr_size(&_recv_dag));
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);

	NeighborTable::iterator it;
	for ( it=_neighborTable.begin() ; it != _neighborTable.end(); it++ ) {
		Xroute::NeighborEntry *n;

		Node p_ad(it->second.AD);
		Node p_hid(it->second.HID);

		//syslog(LOG_DEBUG, "     neighbor: %s", it->second.HID.c_str());

		n   = lsa->add_peers();
		ad  = n->mutable_ad();
		hid = n->mutable_hid();

		ad ->set_type(p_ad.type());
		ad ->set_id(p_ad.id(), XID_SIZE);
		hid->set_type(p_hid.type());
		hid->set_id(p_hid.id(), XID_SIZE);

		n->set_cost(it->second.cost);
		n->set_port(it->second.port);

		if (it->second.AD != _myAD) {
		    n->set_dag(&it->second.dag, sockaddr_size(&it->second.dag));
		}
	}
	return sendMessage(&_controller_dag, msg);
}

// handle the incoming messages
int Router::processMsg(std::string msg_str, uint32_t iface, bool local)
{
	int rc = 0;
	Xroute::XrouteMsg msg;

	try {
		if (!msg.ParseFromString(msg_str)) {
			syslog(LOG_WARNING, "illegal packet received");
			return -1;
		} else if (msg.version() != Xroute::XROUTE_PROTO_VERSION) {
			syslog(LOG_WARNING, "invalid version # received");
			return -1;
		}

	} catch (std::exception e) {
		syslog(LOG_WARNING, "invalid router message received\n");
		return 0;
	}

	switch (msg.type()) {
		case Xroute::KEEPALIVE_MSG:
			rc = processKeepalive(msg.keepalive(), iface);
			break;

		case Xroute::FOREIGN_AD_MSG:
		    if (local) {
		        rc = processForeign(msg.foreign());
		    }
			break;

		case Xroute::TABLE_UPDATE_MSG:
			rc = processRoutingTable(msg);
			break;

		case Xroute::SID_TABLE_UPDATE_MSG:
			processSidRoutingTable(msg);
			break;

		case Xroute::HOST_LEAVE_MSG:
			rc = processHostLeave(msg.host_leave());
			break;

		case Xroute::HOST_JOIN_MSG:
			// process the incoming host-register message
		    if (local) {
			    rc = processHostRegister(msg.host_join());
		    }
		    break;

		case Xroute::CONFIG_MSG:
		    if (local) {
			    rc = processConfig(msg.config());
		    }
			break;

		case Xroute::SID_REQUEST_MSG:
			rc = processSIDRequest(msg);
		    break;

		default:
			syslog(LOG_WARNING, "unsupported routing message received");
			break;
	}

	return rc;
}


int Router::processSidRoutingTable(const Xroute::XrouteMsg& xmsg)
{
	Xroute::SIDTableUpdateMsg msg = xmsg.sid_table_update();
	Xroute::XID fa = msg.from().ad();
	Xroute::XID fh = msg.from().hid();
	Xroute::XID ta = msg.to().ad();
	Xroute::XID th = msg.to().hid();

	string srcAD  = Node(fa.type(), fa.id().c_str(), 0).to_string();
	string srcHID = Node(fh.type(), fh.id().c_str(), 0).to_string();
	string dstAD  = Node(ta.type(), ta.id().c_str(), 0).to_string();
	string dstHID = Node(th.type(), th.id().c_str(), 0).to_string();

	if (srcAD != _myAD || dstAD != _myAD) {
		// FIXME: we shouldn't need this once we have edge detection
		return 1;
	}

	XIARouteTable xrt;
	_xr.getRoutes("AD", xrt);

	// change vector to map AD:RouteEntry for faster lookup
	XIARouteTable ADlookup;
	XIARouteTable::iterator ir;
	for (ir = xrt.begin(); ir != xrt.end(); ++ir) {
		ADlookup[ir->second.xid] = ir->second;
	}

	int rc = 1;
	uint32_t ad_count = msg.ads_size();

	for (uint32_t i = 0; i < ad_count; ++i) {

		Xroute::SIDTableEntry ad_table = msg.ads(i);
		string AD  = Node(ad_table.ad().type(), ad_table.ad().id().c_str(), 0).to_string();
		uint32_t sid_count = ad_table.sids_size();

		XIARouteEntry entry = ADlookup[AD];
		for (uint32_t j = 0; j < sid_count; ++j) {

			Xroute::SIDTableItem item = ad_table.sids(i);
			string SID  = Node(item.sid().type(), item.sid().id().c_str(), 0).to_string();
			uint32_t weight = item.weight();

			//syslog(LOG_INFO, "add route %s, %d, %s, %lu to %s", SID.c_str(), entry.port, entry.nextHop.c_str(), entry.flags, AD.c_str());
			//rc = _xr.delRoute(SID);
			if (weight <= 0) {
				//syslog(LOG_DEBUG, "Removing routing entry: %s@%s", SID.c_str(), entry.xid.c_str());
			}
			if (entry.xid == _myAD) {
				rc = _xr.seletiveSetRoute(SID, -2,  entry.nextHop, entry.flags, weight, AD); // use AD as index
				//SID to local AD, NOTE: no actual server to handle sid here, just put -2 instead, TODO: should point it to a server instance
			} else {
				rc = _xr.seletiveSetRoute(SID, entry.port, entry.nextHop, entry.flags, weight, AD);
			}
			if (rc < 0 ) {
				syslog(LOG_ERR, "error setting sid route %d", rc);
			}
		}
	}
	return rc;
}


int Router::processHostLeave(const Xroute::HostLeaveMsg& msg)
{
	// FIXME: figure out how we do this
	// The controller and every router in the AD have to know the host
	// left so that they can remove it fromm the routing table.
	// there's an additional problem if the host migrated to a new location
	// in the same AD as we need to telll everyone except the new router
	// that the host is gone
	// the new router will advertise the host, but if it's not removed, some
	// routers will get routes to the host's old location

	_xr.delRoute(msg.hid());
	return 1;
}


int Router::processHostRegister(const Xroute::HostJoinMsg& msg)
{
	int rc;
	uint32_t flags;

	if (msg.has_flags()) {
		flags = msg.flags();
	} else {
		flags = F_HOST;
	}

	NeighborEntry neighbor;
	neighbor.AD = _myAD;
	neighbor.HID = msg.hid();
	neighbor.port = msg.interface();
	neighbor.cost = 1; // for now, same cost
	neighbor.flags = flags;
	bzero(&neighbor.dag, sizeof(sockaddr_x));

	// Add host to neighbor table so info can be sent to controller
	_neighborTable[neighbor.HID] = neighbor;

	// update the host entry in (click-side) HID table
	//syslog(LOG_INFO, "Routing table entry: interface=%d, host=%s\n", msg.interface(), hid.c_str());
	if ((rc = _xr.setRoute(neighbor.HID, neighbor.port, neighbor.HID, flags)) != 0) {
		syslog(LOG_ERR, "unable to set host route: %s (%d)", neighbor.HID.c_str(), rc);
	}

	_neighbor_timestamp[neighbor.HID] = time(NULL);

	return 1;
}


int Router::processForeign(const Xroute::ForeignADMsg &msg)
{
	if (msg.ad() == _myAD) {
		return 0;
	}

	// add it to the neighbor table
	NeighborEntry neighbor;

    //syslog(LOG_INFO, "neighbor beacon from %s\n", msg.ad().c_str());

	neighbor.AD    = msg.ad();
	neighbor.HID   = msg.ad();
	neighbor.port  = msg.iface();
	neighbor.flags = 0;
	neighbor.cost  = 1; // for now, same cost
	bzero(&neighbor.dag, sizeof(sockaddr_x));

	// Index by HID if neighbor in same domain or by AD otherwise
	_neighborTable[neighbor.AD] = neighbor;
	_neighbor_timestamp[neighbor.AD] = time(NULL);

	return 1;
}


int Router::processConfig(const Xroute::ConfigMsg &msg)
{
	std::string ad = msg.ad();

	xia_pton(AF_XIA, msg.controller_dag().c_str(), &_controller_dag);

	if (!_joined) {
        //syslog(LOG_INFO, "neighbor beacon from %s\n", msg.ad().c_str());

		// now we can fetch our AD/HID
		if (getXIDs(_myAD, _myHID) < 0) {
			return -1;
		}

        if (_myAD == "") {
            _myAD = ad;
        } else if (_myAD != ad) {
            // this is BAD!
            syslog(LOG_ERR, "AD from xnetj differs from system AD!");
        }

		makeSockets();

		if (XisDualStackRouter(_source_sock) == 1) {
			_flags |= F_IP_GATEWAY;
			//syslog(LOG_DEBUG, "configured as a dual-stack router");
		}

		// we're part of the network now and can start talking
		_joined = true;

	} else if (ad != _myAD) {
		// FIXME: should we nuke the neighbor table?
		// what about the routing tables in click?
	}

	// FIXME: can we get the correct # of interfaces for looping?
	for (int i = 0; i < 8; i++) {
		char el[256];
		sprintf(el, "%s/xlc%d/xarpr", _hostname, i);
		_xr.rawWrite(el, "add", _myAD);
	}

	_xr.setRoute("AD:-",  -7, "", 0x11111111);
	_xr.setRoute("HID:-", -7, "", 0x11111111);
	_xr.setRoute("SID:-", -7, "", 0x11111111);
	_xr.setRoute("IP:-",  -7, "", 0x11111111);

	return 1;
}


int Router::processSIDRequest(Xroute::XrouteMsg &msg)
{
	Xroute::SIDRequestMsg *r = msg.mutable_sid_request();
	r->set_sid(_recv_sid);


	return sendLocalMessage(msg);
}


int Router::processKeepalive(const Xroute::KeepaliveMsg &msg, uint32_t iface)
{
	string neighborAD, neighborHID, neighborSID;
	uint32_t flags = F_HOST;
	NeighborEntry neighbor;
	bool has_sid = msg.node().has_sid() ? true : false;

	Xroute::XID xad  = msg.node().ad();
	Xroute::XID xhid = msg.node().hid();

	Node  ad(xad.type(),  xad.id().c_str(), 0);
	Node hid(xhid.type(), xhid.id().c_str(), 0);
	Node sid;

	neighborAD  = ad. to_string();
	neighborHID = hid.to_string();

	if (has_sid) {
		Xroute::XID xsid = msg.node().sid();
		sid = Node(xsid.type(), xsid.id().c_str(), 0);
		neighborSID = sid.to_string();
		neighbor.HID = neighborSID;

	} else {
		neighbor.HID = neighborHID;
	}

	if (msg.has_flags()) {
		flags = msg.flags();
	}

	// Update neighbor table
	neighbor.AD = neighborAD;
	neighbor.port = iface;
	neighbor.flags = flags;
	neighbor.cost = 1; // for now, same cost
	bzero(&neighbor.dag, sizeof(sockaddr_x));

	// Index by HID if neighbor in same domain or by AD otherwise
	bool internal = (neighbor.AD == _myAD);
	_neighborTable[internal ? neighbor.HID : neighbor.AD] = neighbor;

	_neighbor_timestamp[internal ? neighbor.HID : neighbor.AD] = time(NULL);

	// FIXME: hack until xnetjd deals with hid keepalive routes
	_xr.setRoute(neighborHID, iface, neighborHID, flags);

	//syslog(LOG_INFO, "Process-Keepalive[%s]", neighbor.HID.c_str());

	return 1;
}


int Router::processRoutingTable(const Xroute::XrouteMsg& xmsg)
{
	Xroute::TableUpdateMsg msg = xmsg.table_update();

	Xroute::XID fa = msg.from().ad();
	Xroute::XID fh = msg.from().hid();
	Xroute::XID ta = msg.to().ad();
	Xroute::XID th = msg.to().hid();

	string srcAD  = Node(fa.type(), fa.id().c_str(), 0).to_string();
	string srcHID = Node(fh.type(), fh.id().c_str(), 0).to_string();
	string dstAD  = Node(ta.type(), ta.id().c_str(), 0).to_string();
	string dstHID = Node(th.type(), th.id().c_str(), 0).to_string();

	//syslog(LOG_INFO, "got routing table");

	if (srcAD != _myAD) {
		// FIXME: we shouldn't need this once we have edge detection
		return 1;
	}

	uint32_t numEntries = msg.routes_size();

	for (uint i = 0; i < numEntries; i++) {

		Xroute::TableEntry e = msg.routes(i);
		string xid     = Node(e.xid().type(),      e.xid().id().c_str(),      0).to_string();
		string nextHop = Node(e.next_hop().type(), e.next_hop().id().c_str(), 0).to_string();
		uint32_t port  = e.interface();
		uint32_t flags = 0;

		if (e.has_flags()) {
			flags = e.flags();
		}

		//syslog(LOG_INFO, "got route for %s", xid.c_str());

		int rc;
		if ((rc = _xr.setRoute(xid, port, nextHop, flags)) != 0)
			syslog(LOG_ERR, "error setting route %d: %s, nextHop: %s, port: %d, flags %d", rc, xid.c_str(), nextHop.c_str(), port, flags);

		_route_timestamp[xid] = time(NULL);
	}

	return 1;
}

