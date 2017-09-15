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
		processMsg(string(recv_message, rc));
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
int Host::makeSock(uint32_t iface)
{
	char s[MAX_DAG_SIZE];
	Graph g;

	Node src;
	Node nHID(_myHID);

	// source socket - sending socket
	XmakeNewSID(s, sizeof(s));
	Node outSID(s);
	g = src * nHID * outSID;

	if ((_networks[iface].fd = makeSocket(g, &_networks[iface].source_dag)) < 0) {
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

	h_freq.tv_sec = KEEPALIVE_SECONDS;
	h_freq.tv_usec = KEEPALIVE_MICROSECONDS;

	gettimeofday(&now, NULL);
	timeradd(&now, &h_freq, &h_fire);

	return 0;
}

// send keepalive to router(s)
int Host::sendKeepalive()
{
	int rc = 0;
	string message;
	Xroute::XrouteMsg msg;

	Node n_hid(_myHID);

	Xroute::KeepaliveMsg *ka = msg.mutable_keepalive();
	Xroute::Node *node  = ka->mutable_node();
	Xroute::XID  *hid   = node->mutable_hid();

	msg.set_type(Xroute::KEEPALIVE_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	ka->set_flags(_flags);

	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);

	for (NetworkMap::iterator it = _networks.begin(); it != _networks.end(); it++) {

		Node n_ad(it->second.ad);
		Xroute::XID *ad = node->mutable_ad();

		ad ->set_type(n_ad.type());
		ad ->set_id(n_ad.id(), XID_SIZE);

		if ((rc = sendMessage(it->second.fd, &it->second.router_dag, msg)) < 0) {
			break;
		}
	}

	return rc;
}


// handle the incoming messages
int Host::processMsg(std::string msg_str)
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
		case Xroute::HOST_CONFIG_MSG:
			rc = processConfig(msg.host_config());
			break;

		default:
			syslog(LOG_WARNING, "unsupported routing message received");
			break;
	}

	return rc;
}


int Host::processConfig(const Xroute::HostConfigMsg &msg)
{
	std::string a;
	std::string rdag;
	NetInfo n;

	uint32_t iface = msg.iface();

	syslog(LOG_INFO, "iface = %d\n", iface);

	n.ad = msg.ad();

	rdag = "RE " + msg.hid() + " " + msg.sid();
	xia_pton(AF_XIA, rdag.c_str(), &n.router_dag);

	// throw ad away since it won't be valid if we're multihomed
	// we have the correct ad in the msg, so we're still ok
	if (getXIDs(a, _myHID) < 0) {
		return -1;
	}

	char s[MAX_DAG_SIZE];
	Graph g;

	Node src;
	Node nHID(_myHID);

	// source socket - sending socket
	XmakeNewSID(s, sizeof(s));
	Node outSID(s);
	g = src * nHID * outSID;

	if ((n.fd = makeSocket(g, &n.source_dag)) < 0) {
		return -1;
	}

	syslog(LOG_INFO, "Joined %s with Source socket: %s", n.ad.c_str(), g.dag_string().c_str());

	_networks[iface] = n;

	// we're part of the network now and can start talking
	_joined = true;

	return 1;
}

