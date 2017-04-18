//#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>

#include "../common/XIARouter.hh"
#include "Router.hh"
#include "dagaddr.hpp"

// FIXME:
// import sid routehandler?
// does the dual router flag serve any purpose?
// implement host leave functionality so that all routers correctly remove the host
// implement security!!!
//

void Router::purge()
{
	// initialize if this is first time in
	if (_last_route_purge == 0) {
		_last_route_purge = time(NULL);
		_last_neighbor_purge = time(NULL);
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

void Router::sendMessages()
{
	struct timeval now;
	gettimeofday(&now, NULL);

	if (timercmp(&now, &h_fire, >=)) {
		sendHello();
		timeradd(&now, &h_freq, &h_fire);
	}
	if (timercmp(&now, &l_fire, >=)) {
		sendLSA();
		timeradd(&now, &l_freq, &l_fire);
	}
}

#define BUFFER_SIZE 16384

int Router::readMessage(char *recv_message, int *iface)
{
	int rc = -1;
	sockaddr_x theirDAG;
	struct pollfd pfd[3];
	struct timespec tspec;

	bzero(pfd, sizeof(pfd));
	pfd[0].fd = _local_sock;
	pfd[1].fd = _broadcast_sock;
	pfd[2].fd = _router_sock;
	pfd[0].events = POLLIN;
	pfd[1].events = POLLIN;
	pfd[2].events = POLLIN;

	tspec.tv_sec = 0;
	tspec.tv_nsec = 500000000;

	// only poll for local sock if we haven't gotten config info from the network
	int num_pfds = _joined ? 3 : 1;

	rc = Xppoll(pfd, num_pfds, &tspec, NULL);
	if (rc > 0) {
			int sock = -1;

		for (int i = 0; i < num_pfds; i++) {
			if (pfd[i].revents & POLLIN) {
				sock = pfd[i].fd;
				break;
			}
		}

		bzero(recv_message, BUFFER_SIZE);

		if (sock <= 0) {
			// something weird happened
			return 0;

		} else if (sock == _local_sock) {
			// it's  a normal UDP socket
			iface = 0;
			if ((rc = recvfrom(sock, recv_message, BUFFER_SIZE, 0, NULL, NULL)) < 0) {
				syslog(LOG_WARNING, "local message receive error");
			}
		} else {
			// it's an Xsocket, use Xrecvmsg to get interface message came in on

			struct msghdr mh;
			struct iovec iov;
			struct in_pktinfo pi;
			struct cmsghdr *cmsg;
			struct in_pktinfo *pinfo;
			char cbuf[CMSG_SPACE(sizeof pi)];

			iov.iov_base = recv_message;
			iov.iov_len = BUFFER_SIZE;

			mh.msg_name = &theirDAG;
			mh.msg_namelen = sizeof(theirDAG);
			mh.msg_iov = &iov;
			mh.msg_iovlen = 1;
			mh.msg_control = cbuf;
			mh.msg_controllen = sizeof(cbuf);

			cmsg = CMSG_FIRSTHDR(&mh);
			cmsg->cmsg_level = IPPROTO_IP;
			cmsg->cmsg_type = IP_PKTINFO;
			cmsg->cmsg_len = CMSG_LEN(sizeof(pi));

			mh.msg_controllen = cmsg->cmsg_len;

			if ((rc = Xrecvmsg(sock, &mh, 0)) < 0) {
				perror("recvfrom");

			} else {
				for (cmsg = CMSG_FIRSTHDR(&mh); cmsg != NULL; cmsg = CMSG_NXTHDR(&mh, cmsg)) {
					if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
						pinfo = (struct in_pktinfo*) CMSG_DATA(cmsg);
						*iface = pinfo->ipi_ifindex;
					}
				}
			}
		}
	}

	return rc;
}

void *Router::handler()
{
	int rc;
	char recv_message[BUFFER_SIZE];
	vector<string> routers;
	int iface;

	// get the next incoming message
	if ((rc = readMessage(recv_message, &iface)) > 0) {
		processMsg(string(recv_message, rc), iface);
	}


	if (_joined) {
		// once we are initialized, purge any stale routes
		//  and start sending hello's and lsa's
		purge();
		sendMessages();
	}

	return NULL;
}

int Router::postJoin()
{
	char s[MAX_DAG_SIZE];
	Graph g;
	Node src;

	// broadcast socket - hello messages, can hopefully go away
	_broadcast_sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (_broadcast_sock < 0) {
		syslog(LOG_ALERT, "Unable to create the broadcast socket");
		return -1;
	}

	// router socket - flooded & interdomain communication
	_router_sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (_router_sock < 0) {
		syslog(LOG_ALERT, "Unable to create the controller socket");
		return -1;
	}

	// source socket - sending socket
	_source_sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (_source_sock < 0) {
		syslog(LOG_ALERT, "Unable to create the out socket");
		return -1;
	}

	// get our AD and HID
	if ( XreadLocalHostAddr(_source_sock, s, MAX_DAG_SIZE, NULL, 0) < 0 ) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		return -1;
	}

	// FIXME: these should probably be Nodes instead of strings!
	Graph gbroad(s);
	_myAD = gbroad.intent_AD_str();
	_myHID = gbroad.intent_HID_str();

	Node nHID(_myHID);

	// make the dag we'll receive broadcasts on
	g = src * Node(broadcast_fid) * Node(intradomain_sid);

	g.fill_sockaddr(&_broadcast_dag);
	syslog(LOG_INFO, "Broadcast DAG: %s", g.dag_string().c_str());

	// and bind it to the broadcast receive socket
	if (Xbind(_broadcast_sock, (struct sockaddr*)&_broadcast_dag, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ALERT, "unable to bind to broadcast receive DAG : %s", g.dag_string().c_str());
		return -1;
	}

	// make the dag we'll receive controller messages on
	XcreateFID(s, sizeof(s));

	Node nFID(s);

	XmakeNewSID(s, sizeof(s));

	Node rSID(s);
	g = (src * nHID * nFID * rSID) + (src * nFID * rSID);

	g.fill_sockaddr(&_router_dag);
	syslog(LOG_INFO, "Control Receiver DAG: %s", g.dag_string().c_str());

	// and bind it to the controller receive socket
	if (Xbind(_router_sock, (struct sockaddr*)&_router_dag, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ALERT, "unable to bind to controller DAG : %s", g.dag_string().c_str());
		return -1;
	}


	// make the DAG we'll send with
	XmakeNewSID(s, sizeof(s));

	Node nAD(_myAD);
	Node outSID(s);
	g = src * nAD * nHID * outSID;

	g.fill_sockaddr(&_source_dag);
	syslog(LOG_INFO, "Source DAG: %s", g.dag_string().c_str());

	// and bind it to the source socket
	if (Xbind(_source_sock, (struct sockaddr*)&_source_dag, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ALERT, "unable to bind to controller DAG : %s", g.dag_string().c_str());
		return -1;
	}

	if( XisDualStackRouter(_source_sock) == 1 ) {
		_flags |= F_IP_GATEWAY;
		syslog(LOG_DEBUG, "configured as a dual-stack router");
	}

	// we're part of the network now and can start talking
	_joined = true;

	return 0;
}


int Router::init()
{
	_joined = false; 				// we're not part of a network yet

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

int Router::sendHello()
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

// send LinkStateAdvertisement message (flooding)
int Router::sendLSA()
{
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
	lsa->set_dag(&_router_dag, sockaddr_size(&_router_dag));
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);

	NeighborTable::iterator it;
	for ( it=_neighborTable.begin() ; it != _neighborTable.end(); it++ ) {
		Xroute::NeighborEntry *n;

		Node p_ad(it->second.AD);
		Node p_hid(it->second.HID);

		n   = lsa->add_peers();
		ad  = n->mutable_ad();
		hid = n->mutable_hid();

		ad ->set_type(p_ad.type());
		ad ->set_id(p_ad.id(), XID_SIZE);
		hid->set_type(p_hid.type());
		hid->set_id(p_hid.id(), XID_SIZE);

		n->set_cost(it->second.cost);
		n->set_port(it->second.port);
	}

	return sendMessage(&_controller_dag, msg);
}

int Router::processMsg(std::string msg_str, uint32_t iface)
{
	int rc = 0;
	Xroute::XrouteMsg msg;

	if (!msg.ParseFromString(msg_str)) {
		syslog(LOG_WARNING, "illegal packet received");
		return -1;
	} else if (msg.version() != Xroute::XROUTE_PROTO_VERSION) {
		syslog(LOG_WARNING, "invalid version # received");
		return -1;
	}

	switch (msg.type()) {
		case Xroute::HELLO_MSG:
			// process the incoming Hello message
			rc = processHello(msg.hello(), iface);
			break;

		case Xroute::TABLE_UPDATE_MSG:
			rc = processRoutingTable(msg);
			break;

		case Xroute::SID_TABLE_UPDATE_MSG:
			processSidRoutingTable(msg);
			break;

		// FIXME: validate these came on control interface?
		case Xroute::HOST_JOIN_MSG:
			// process the incoming host-register message
			rc = processHostRegister(msg.host_join());
			break;

		case Xroute::HOST_LEAVE_MSG:
			rc = processHostLeave(msg.host_leave());
			break;

		case Xroute::CONFIG_MSG:
			rc = processConfig(msg.config());
			break;

		default:
			syslog(LOG_INFO, "unknown routing message type");
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

	// Add host to neighbor table so info can be sent to controller
	_neighborTable[neighbor.HID] = neighbor;

	//  update my entry in the networkTable
	NodeStateEntry entry;
	entry.hid = _myHID;
	entry.ad = _myAD;

	// fill my neighbors into my entry in the networkTable
	NeighborTable::iterator it;
	for (it = _neighborTable.begin(); it != _neighborTable.end(); it++)
		entry.neighbor_list.push_back(it->second);

	_networkTable[_myHID] = entry;

	// update the host entry in (click-side) HID table
	//syslog(LOG_INFO, "Routing table entry: interface=%d, host=%s\n", msg.interface(), hid.c_str());
	if ((rc = _xr.setRoute(neighbor.HID, neighbor.port, neighbor.HID, flags)) != 0) {
		syslog(LOG_ERR, "unable to set host route: %s (%d)", neighbor.HID.c_str(), rc);
	}

	_neighbor_timestamp[neighbor.HID] = time(NULL);

	return 1;
}

int Router::processConfig(const Xroute::ConfigMsg &msg)
{
	_myAD = msg.ad();

	xia_pton(AF_XIA, msg.controller_dag().c_str(), &_controller_dag);

	// the default AD & HID got set when xnetjoin setup our AD, reset them to fallback
	_xr.setRoute("AD:-", FALLBACK, "", 0);
	_xr.setRoute("HID:-", FALLBACK, "", 0);

	postJoin();
	return 1;
}

int Router::processHello(const Xroute::HelloMsg &msg, uint32_t iface)
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

	_networkTable[_myHID] = entry;

	// FIXME: hack until xnetjd deals with hid hello routes
	_xr.setRoute(neighborHID, iface, neighborHID, flags);

	//syslog(LOG_INFO, "Process-Hello[%s]", neighbor.HID.c_str());

	return 1;
}

int Router::processLSA(const Xroute::XrouteMsg& msg)
{
	// FIXME: we still need to reflood until the FID logic is switched out of broadcast mode

	string neighborAD, neighborHID;
	string srcAD, srcHID;


// FIXME: we should never get here!

	const Xroute::LSAMsg& lsa = msg.lsa();

	Xroute::XID a = lsa.node().ad();
	Xroute::XID h = lsa.node().hid();

	Node  ad(a.type(), a.id().c_str(), 0);
	Node hid(h.type(), h.id().c_str(), 0);

	srcAD  = ad.to_string();
	srcHID = hid.to_string();

	syslog(LOG_INFO, "Process-LSA: %s %s", srcAD.c_str(), srcHID.c_str());

	if (srcHID.compare(_myHID) == 0) {
		// skip if from me
		return 1;
	}

	return 1;
//	syslog(LOG_INFO, "Forward-LSA: %s %s", srcAD.c_str(), srcHID.c_str());
//	return sendMessage(_sock, &_ddag, msg);
}


// process a control message
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

		int rc;
		if ((rc = _xr.setRoute(xid, port, nextHop, flags)) != 0)
			syslog(LOG_ERR, "error setting route %d: %s, nextHop: %s, port: %d, flags %d", rc, xid.c_str(), nextHop.c_str(), port, flags);

		_route_timestamp[xid] = time(NULL);
	}

	return 1;
}
