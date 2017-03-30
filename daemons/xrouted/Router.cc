//#include <signal.h>
#include <sys/time.h>
#include <math.h>

#include "../common/XIARouter.hh"
#include "Router.hh"
#include "dagaddr.hpp"

// FIXME:
// move setup info from constructor into a method and call it from run
// pass info between modules
// make a routercore class for xiarouter and passing info between modules
// what is the difference between the purge and purge host loops?
// import sid routehandler?
// does the dual router flag serve any purpose?
// should we support an old style (controllerless) route daemon?

// FIXME: on new source tree
// modify XreradLocalAddr to not require a 4id
// make the xiarouter return a map instead of vector


int Router::getControllerDag(sockaddr_x *dag)
{
	// make the dag we'll send controller messages to
	// FIXME: eventually we'll get this from xnetjd

	sockaddr_x ns;

	XreadNameServerDAG(_local_sock, &ns);

	Graph g(&ns);

	Node src;
	Node h = g.intent_HID();
	Node f(controller_fid);
	Node s(controller_sid);
	g = (src * h * f * s) + (src * f * s);

	g.fill_sockaddr(dag);
	syslog(LOG_INFO, "Controller DAG: %s", g.dag_string().c_str());
	return 0;
}

// FIXME: put this somewhere common
static uint32_t sockaddr_size(const sockaddr_x *s)
{
	// max possible size
	size_t len = sizeof(sockaddr_x);

	// subtract the space occupied by unallocated nodes
	len -= (CLICK_XIA_ADDR_MAX_NODES - s->sx_addr.s_count) * sizeof(node_t);

	return len;
}

void *Router::handler()
{
	int rc;
	char recv_message[10240];
	sockaddr_x theirDAG;
	struct timespec tspec;
	vector<string> routers;
	time_t last_purge, hello_last_purge;

	struct timeval now;

	last_purge = hello_last_purge = time(NULL);
	{
	//while (1) {
		gettimeofday(&now, NULL);
		if (timercmp(&now, &h_fire, >=)) {
			sendHello();
			timeradd(&now, &h_freq, &h_fire);
		}
		if (timercmp(&now, &l_fire, >=)) {
			sendLSA();
			timeradd(&now, &l_freq, &l_fire);
		}

		struct pollfd pfd[3];

		bzero(pfd, sizeof(pfd));
		pfd[0].fd = _broadcast_sock;
		pfd[1].fd = _router_sock;
		pfd[2].fd = _local_sock;
		pfd[0].events = POLLIN;
		pfd[1].events = POLLIN;
		pfd[2].events = POLLIN;

		tspec.tv_sec = 0;
		tspec.tv_nsec = 500000000;

		rc = Xppoll(pfd, 1, &tspec, NULL);
		if (rc > 0) {
				int sock = -1;

			for (int i = 0; i < 3; i++) {
				if (pfd[i].revents & POLLIN) {
					sock = pfd[i].fd;
					break;
				}
			}

			if (sock < 0) {
				// something weird happened
				return 0;
			}


			// receiving a Hello or LSA packet
			memset(&recv_message[0], 0, sizeof(recv_message));

			int iface;
			struct msghdr mh;
			struct iovec iov;
			struct in_pktinfo pi;
			struct cmsghdr *cmsg;
			struct in_pktinfo *pinfo;
			char cbuf[CMSG_SPACE(sizeof pi)];

			iov.iov_base = recv_message;
			iov.iov_len = sizeof(recv_message);

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
						iface = pinfo->ipi_ifindex;
					}
				}
				processMsg(string(recv_message, rc), iface);
			}
		}

		time_t nowt = time(NULL);
		if (nowt - last_purge >= EXPIRE_TIME) {
			last_purge = nowt;
			//fprintf(stderr, "checking entry\n");
			map<string, time_t>::iterator iter = timeStamp.begin();

			while (iter != timeStamp.end()) {
				if (nowt - iter->second >= EXPIRE_TIME) {
					_xr.delRoute(iter->first);
					syslog(LOG_INFO, "purging host route for : %s", iter->first.c_str());
					timeStamp.erase(iter++);
				} else {
					++iter;
				}
			}
		}

		if (nowt - hello_last_purge >= HELLO_EXPIRE_TIME) {
			hello_last_purge = nowt;
			//fprintf(stderr, "checking hello entry\n");
			map<string, time_t>::iterator iter = _hello_timeStamp.begin();

			while (iter !=  _hello_timeStamp.end()) {
				if (nowt - iter->second >= HELLO_EXPIRE_TIME) {
					_xr.delRoute(iter->first);
					syslog(LOG_INFO, "purging hello route for : %s", iter->first.c_str());


					// Update network table

					// remove the item from neighbor_list
					_networkTable[_myHID].neighbor_list.erase(
						std::remove(_networkTable[_myHID].neighbor_list.begin(), _networkTable[_myHID].neighbor_list.end(), _neighborTable[iter->first]),
						_networkTable[_myHID].neighbor_list.end());

					if (_neighborTable.erase(iter->first) < 1) {
						 syslog(LOG_INFO, "failed to erase %s from neighbors", iter->first.c_str());
					}
					_num_neighbors = _neighborTable.size();
					_networkTable[_myHID].num_neighbors = _num_neighbors;

					// remove the item and go on
					_hello_timeStamp.erase(iter++);
				} else {
					++iter;
				}
			}
		}
	}
	return 0;
}

int Router::makeSockets()
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

	// control socket - local communication
	_local_sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (_local_sock < 0) {
		syslog(LOG_ALERT, "Unable to create the local control socket");
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
	if ( XreadLocalHostAddr(_local_sock, s, MAX_DAG_SIZE, NULL, 0) < 0 ) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		return -1;
	}

	// FIXME: these should probably be Nodes instead of strings!
	Graph glocal(s);
	_myAD = glocal.intent_AD_str();
	_myHID = glocal.intent_HID_str();

	Node nHID(_myHID);


	// make the dag we'll receive local broadcasts on
	g = src * Node(broadcast_fid) * Node(intradomain_sid);

	g.fill_sockaddr(&_broadcast_dag);
	syslog(LOG_INFO, "Broadcast DAG: %s", g.dag_string().c_str());

	// and bind it to the broadcast receive socket
	if (Xbind(_broadcast_sock, (struct sockaddr*)&_broadcast_dag, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		return -1;
	}

	// make the dag we'll receive local messages on
	g = src * nHID * Node(local_sid);

	g.fill_sockaddr(&_local_dag);
	syslog(LOG_INFO, "Local DAG: %s", g.dag_string().c_str());

	// and bind it to the broadcast receive socket
	if (Xbind(_local_sock, (struct sockaddr*)&_local_dag, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		return -1;
	}

	// make the dag we'll receive controller messages on
	XcreateFID(s, sizeof(s));

	Node nFID(s);

	XmakeNewSID(s, sizeof(s));

	Node rSID(s);
	g = (src * nHID * nFID * rSID) + (src * nFID * rSID);

	g.fill_sockaddr(&_router_dag);
	syslog(LOG_INFO, "Controller DAG: %s", g.dag_string().c_str());

	// and bind it to the controller receive socket
	if (Xbind(_router_sock, (struct sockaddr*)&_router_dag, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ALERT, "unable to bind to controller DAG : %s", g.dag_string().c_str());
		return -1;
	}


	// make the DAG we'll send with
	XmakeNewSID(s, sizeof(s));
	Node nAD(_myAD);
	Node outSID(s);
	g = nAD * nHID * outSID;

	g.fill_sockaddr(&_source_dag);
	syslog(LOG_INFO, "Source DAG: %s", g.dag_string().c_str());

	// and bind it to the source socket
	if (Xbind(_source_sock, (struct sockaddr*)&_source_dag, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ALERT, "unable to bind to controller DAG : %s", g.dag_string().c_str());
		return -1;
	}

	getControllerDag(&_controller_dag);

	return 0;
}


int Router::init()
{
	if (makeSockets() < 0) {
		exit(-1);
	}

	_num_neighbors = 0;				// number of seen neighbor routers
	_lsa_seq = rand() % MAX_SEQNUM;	// make our initial LSA sequence numbe

	_ctl_seq = 0;	// LSA sequence number of this router

	// FIXME: figure out what the correct type of router we are
	_flags = F_EDGE_ROUTER;

	if( XisDualStackRouter(_local_sock) == 1 ) {
		_flags |= F_IP_GATEWAY;
		syslog(LOG_DEBUG, "configured as a dual-stack router");
	}

	struct timeval now;

	h_freq.tv_sec = 0;
	h_freq.tv_usec = 100000;
	l_freq.tv_sec = 0;
	l_freq.tv_usec = 300000;

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
	hello->set_dag(&_router_dag, sockaddr_size(&_router_dag));
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
	// don't bother if our neighbor table is empty
	if (_neighborTable.size() == 0) {
		return 1;
	}

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

printf("send lsa size = %lu\n", _neighborTable.size());

	map<std::string, NeighborEntry>::iterator it;
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

//	printf("sending %s\n", msg.DebugString().c_str());
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
		printf("got update\n");
			rc = processRoutingTable(msg);
			break;

		case Xroute::HOST_LEAVE_MSG:
			// FIXME: should this move to the controller?
			break;

		case Xroute::SID_TABLE_UPDATE_MSG:
		printf("got sid update\n");
			processSidRoutingTable(msg);
			break;

		//////////////////////////////////////////////////////////////////////
		// FIXME: move these to the control interface
		case Xroute::HOST_JOIN_MSG:
			// process the incoming host-register message
			rc = processHostRegister(msg.host_join());
			break;

		case Xroute::CONFIG_MSG:
			break;

		//////////////////////////////////////////////////////////////////////
		// We should never see these as they are meant for the controller!
		case Xroute::LSA_MSG:
			// FIXME: this can be no-op'd once flooding is turned on
		printf("got lsa\n");

			rc = processLSA(msg);
			break;

		// not sure all of these below were ever implemented
		case Xroute::GLOBAL_LSA_MSG:
		case Xroute::SID_DISCOVERY_MSG:
		case Xroute::SID_MANAGE_KA_MSG:
		case Xroute::SID_RE_DISCOVERY_MSG:
		case Xroute::AD_PATH_STATE_PING_MSG:
		case Xroute::AD_PATH_STATE_PONG_MSG:
		case Xroute::SID_DECISION_QUERY_MSG:
		case Xroute::SID_DECISION_ANSWER_MSG:
			syslog(LOG_INFO, "this is a controller message");
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


//	// FIXME: we shoudn't need this once flooding is turned on
//	if (dstHID != _myHID) {
//		return sendMessage(_sock, &_ddag, xmsg);
//	}

	std::map<std::string, XIARouteEntry> xrt;
	_xr.getRoutes("AD", xrt);

	// change vector to map AD:RouteEntry for faster lookup
	std::map<std::string, XIARouteEntry> ADlookup;
	map<std::string, XIARouteEntry>::iterator ir;
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
	_num_neighbors = _neighborTable.size();

	//  update my entry in the networkTable
	NodeStateEntry entry;
	entry.hid = _myHID;
	entry.ad = _myAD;
	entry.num_neighbors = _num_neighbors;

	// fill my neighbors into my entry in the networkTable
	std::map<std::string, NeighborEntry>::iterator it;
	for (it = _neighborTable.begin(); it != _neighborTable.end(); it++)
		entry.neighbor_list.push_back(it->second);

	_networkTable[_myHID] = entry;

	// update the host entry in (click-side) HID table
	//syslog(LOG_INFO, "Routing table entry: interface=%d, host=%s\n", msg.interface(), hid.c_str());
	if ((rc = _xr.setRoute(neighbor.HID, neighbor.port, neighbor.HID, flags)) != 0) {
		syslog(LOG_ERR, "unable to set host route: %s (%d)", neighbor.HID.c_str(), rc);
	}

	_hello_timeStamp[neighbor.HID] = time(NULL);

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

		printf("has sid\n");
	} else {
		neighbor.HID = neighborHID;
	}

	if (msg.has_dag()) {
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
	_num_neighbors = _neighborTable.size();

	_hello_timeStamp[internal ? neighbor.HID : neighbor.AD] = time(NULL);

	// Update network table
	NodeStateEntry entry;
	entry.hid = _myHID;
	entry.ad = _myAD;
	entry.num_neighbors = _num_neighbors;

	// Add neighbors to network table entry
	std::map<std::string, NeighborEntry>::iterator it;
	for (it = _neighborTable.begin(); it != _neighborTable.end(); it++)
		entry.neighbor_list.push_back(it->second);

	_networkTable[_myHID] = entry;

	printf("network table size = %lu\n", _networkTable.size());
	printf("neighbor table size = %lu\n", _neighborTable.size());
	printf("entry.neighborlist size = %lu\n", entry.neighbor_list.size());


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
printf("processLSA: %s\n", msg.DebugString().c_str());

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

printf("processRoutingTable 1\n");

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

printf("processRoutingTable 2\n");
//	// FIXME: we shoudn't need this once flooding is turned on
//	if ((dstAD != _myAD) || (dstHID != _myHID)) {
//		return sendMessage(_sock, &_ddag, xmsg);
//	}

	uint32_t numEntries = msg.routes_size();

	for (uint i = 0; i < numEntries; i++) {
printf("processRoutingTable 3\n");

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

		_timeStamp[xid] = time(NULL);
	}
	return 1;
}
