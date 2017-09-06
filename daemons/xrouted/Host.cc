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

int Host::handler()
{
	int rc;
	char recv_message[BUFFER_SIZE];
	struct pollfd pfd;
	int iface;

	bzero(&pfd, sizeof(pfd));
	pfd.fd = _local_sock;
	pfd.events = POLLIN;

	// get the next incoming message
	if ((rc = readMessage(recv_message, &pfd, 1, &iface)) > 0) {
		processMsg(string(recv_message, rc), iface);
	}

	if (_joined) {
		struct timeval now;
		gettimeofday(&now, NULL);

		if (timercmp(&now, &h_fire, >=)) {
			sendKeepalive();
			timeradd(&now, &h_freq, &h_fire);
		}
	}

	return 0;
}


// FIXME: deal with multihoming!!!
// now that we've joined the network, set up our dags and sockets
int Host::makeSockets()
{
	char s[MAX_DAG_SIZE];
	Graph g;

	Node src;
	Node nHID(_myHID);
	Node nAD(_myAD);

	// source socket - sending socket
	XmakeNewSID(s, sizeof(s));
	Node outSID(s);
	g = src * nAD * nHID * outSID;

	if ((_source_sock = makeSocket(g, &_source_dag)) < 0) {
		return -1;
	}

	syslog(LOG_INFO, "Source socket: %s", g.dag_string().c_str());

	return 0;
}


int Host::init()
{
	_joined = false; // we're not part of a network yet

	_flags = F_HOST;

	struct timeval now;

	h_freq.tv_sec = 0;
	h_freq.tv_usec = 100000;

	gettimeofday(&now, NULL);
	timeradd(&now, &h_freq, &h_fire);

	return 0;
}

// send keepalive to router(s)
// FIXME: make sure to do this for each interface
int Host::sendKeepalive()
{
	string message;

	Node n_ad(_myAD);
	Node n_hid(_myHID);

	Xroute::XrouteMsg msg;
	Xroute::KeepaliveMsg *ka = msg.mutable_keepalive();
	Xroute::Node *node  = ka->mutable_node();
	Xroute::XID  *ad    = node->mutable_ad();
	Xroute::XID  *hid   = node->mutable_hid();

	msg.set_type(Xroute::KEEPALIVE_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	ka->set_flags(_flags);
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);

	return sendMessage(&_router_dag, msg);
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
		case Xroute::HOST_CONFIG_MSG:
			rc = processConfig(msg.host_config());
			break;

		default:
			syslog(LOG_INFO, "unknown routing message type");
			break;
	}

	return rc;
}


int Host::processConfig(const Xroute::HostConfigMsg &msg)
{
	std::string dag;
	std::string ad  = msg.ad();
	std::string hid = msg.hid();
	std::string sid = msg.sid();
	uint32_t iface  = msg.iface();

	dag = "RE " + hid + " " + sid;

	xia_pton(AF_XIA, dag.c_str(), &_router_dag);

	if (!_joined) {
		// now we can fetch our AD/HID
		if (getXIDs(_myAD, _myHID) < 0) {
			return -1;
		}

		makeSockets();

		// we're part of the network now and can start talking
		_joined = true;

	} else if (ad != _myAD) {
		// FIXME: should we nuke the neighbor table?
		// what about the routing tables in click?
	}

	return 1;
}

