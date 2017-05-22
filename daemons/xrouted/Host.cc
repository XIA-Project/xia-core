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
#include "Host.hh"
#include "dagaddr.hpp"

// FIXME:
// import sid routehandler?
// does the dual router flag serve any purpose?
// implement security!!!


// purge stale entries from our tables
void Host::purge()
{
	// initialize if this is first time in
	if (_last_neighbor_purge == 0) {
		_last_neighbor_purge = time(NULL);
	    return;
	}

	time_t now = time(NULL);

	// walk list of neighbors we've discovered via hello
	//  and delete any that are stale
	if (now - _last_neighbor_purge >= NEIGHBOR_EXPIRE_TIME) {
		_last_neighbor_purge = now;

		TimestampList::iterator iter = _neighbor_timestamp.begin();
		while (iter !=  _neighbor_timestamp.end()) {

			if (now - iter->second >= NEIGHBOR_EXPIRE_TIME) {
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


int Host::handler()
{
	int rc;
	char recv_message[BUFFER_SIZE];
	struct pollfd pfds[3];
	int iface;

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
	if ((rc = readMessage(recv_message, pfds, num_pfds, &iface)) > 0) {
		processMsg(string(recv_message, rc), iface);
	}

	if (_joined) {
		// once we are initialized, purge any stale routes
		//  and start sending hello's and lsa's
		purge();

		struct timeval now;
		gettimeofday(&now, NULL);

		if (timercmp(&now, &h_fire, >=)) {
			sendHello();
			timeradd(&now, &h_freq, &h_fire);
		}
	}

	return 0;
}


// now that we've joined the network, set up our dags and sockets
int Host::makeSockets()
{
	char s[MAX_DAG_SIZE];
	Graph g;

	Node src;
	Node nHID(_myHID);
	Node nAD(_myAD);

	// broadcast socket - hello messages, can hopefully go away
	g = src * Node(broadcast_fid) * Node(intradomain_sid);
	if ((_broadcast_sock = makeSocket(g, &_broadcast_dag)) < 0) {
		return -1;
	}

	// router socket - flooded & interdomain communication
	XcreateFID(s, sizeof(s));
	Node nFID(s);
	XmakeNewSID(s, sizeof(s));
	Node rSID(s);
	g = (src * nHID * nFID * rSID) + (src * nFID * rSID);

	if ((_recv_sock = makeSocket(g, &_recv_dag)) < 0) {
		return -1;
	}

	// source socket - sending socket
	XmakeNewSID(s, sizeof(s));
	Node outSID(s);
	g = src * nAD * nHID * outSID;

	if ((_source_sock = makeSocket(g, &_source_dag)) < 0) {
		return -1;
	}

	syslog(LOG_INFO, "Broadcast: %s", g.dag_string().c_str());
	syslog(LOG_INFO, "Flood: %s", g.dag_string().c_str());
	syslog(LOG_INFO, "Source: %s", g.dag_string().c_str());

	return 0;
}


int Host::init()
{
	_joined = false; // we're not part of a network yet

	// FIXME: figure out what the correct type of router we are
	_flags = F_EDGE_ROUTER;

	struct timeval now;

	h_freq.tv_sec = 0;
	h_freq.tv_usec = 100000;

	_last_neighbor_purge = 0;

	gettimeofday(&now, NULL);
	timeradd(&now, &h_freq, &h_fire);

	return 0;
}

// send hello on braodcast interface
// will eventually go away when netjoin does this for us
int Host::sendHello()
{
	// FIXME: removed sending SIDs for now, do we need to in the future???
	string message;

	Node n_ad(_myAD);
	Node n_hid(_myHID);
	Node n_sid(intradomain_sid);

	Xroute::XrouteMsg msg;
	Xroute::HelloMsg *hello = msg.mutable_hello();
	Xroute::Node     *node  = hello->mutable_node();
	Xroute::XID      *ad    = node->mutable_ad();
	Xroute::XID      *hid   = node->mutable_hid();
	//Xroute::XID      *sid   = node->mutable_sid();

	msg.set_type(Xroute::HELLO_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	hello->set_flags(_flags);
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);
	// sid->set_type(n_sid.type());
	// sid->set_id(n_sid.id(), XID_SIZE);

	return sendBroadcastMessage(msg);
}


// handle the incoming messages
int Host::processMsg(std::string msg_str, uint32_t iface)
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
		syslog(LOG_INFO, "invalid router message received\n");
		return 0;
	}

	switch (msg.type()) {
		case Xroute::HELLO_MSG:
			rc = processHello(msg.hello(), iface);
			break;

		case Xroute::CONFIG_MSG:
			rc = processConfig(msg.config());
			break;

	    case Xroute::SID_REQUEST_MSG:
			rc = processSIDRequest(msg);
	        break;

		default:
			syslog(LOG_INFO, "unknown routing message type");
			break;
	}

	return rc;
}


int Host::processConfig(const Xroute::ConfigMsg &msg)
{
	std::string ad = msg.ad();

	xia_pton(AF_XIA, msg.controller_dag().c_str(), &_controller_dag);

	if (!_joined) {
		// now we can fetch our AD/HID
		if (getXIDs(_myAD, _myHID) < 0) {
			return -1;
		}

		makeSockets();

		if (XisDualStackRouter(_source_sock) == 1) {
			_flags |= F_IP_GATEWAY;
			syslog(LOG_DEBUG, "configured as a dual-stack router");
		}

		// we're part of the network now and can start talking
		_joined = true;

	} else if (ad != _myAD) {
		// FIXME: should we nuke the neighbor table?
		// what about the routing tables in click?
	}

	// netjoin causes the default routes to be treated as if this
	// is a host and not a router. Put them back to normal.
	_xr.setRoute("AD:-", FALLBACK, "", 0);
	_xr.setRoute("HID:-", FALLBACK, "", 0);

	return 1;
}


int Host::processSIDRequest(Xroute::XrouteMsg &msg)
{
	Xroute::SIDRequestMsg *r = msg.mutable_sid_request();
	r->set_sid(_recv_sid);


	return sendLocalMessage(msg);
}


int Host::processHello(const Xroute::HelloMsg &msg, uint32_t iface)
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

	// Index by HID if neighbor in same domain or by AD otherwise
	bool internal = (neighbor.AD == _myAD);
	_neighborTable[internal ? neighbor.HID : neighbor.AD] = neighbor;

	_neighbor_timestamp[internal ? neighbor.HID : neighbor.AD] = time(NULL);

	// Update network table
	NodeStateEntry entry;
	entry.hid = _myHID;
	entry.ad = _myAD;

	// Add neighbors to network table entry
	NeighborTable::iterator it;
	for (it = _neighborTable.begin(); it != _neighborTable.end(); it++)
		entry.neighbor_list.push_back(it->second);

	// FIXME: hack until xnetjd deals with hid hello routes
	_xr.setRoute(neighborHID, iface, neighborHID, flags);

	//syslog(LOG_INFO, "Process-Hello[%s]", neighbor.HID.c_str());

	return 1;
}

