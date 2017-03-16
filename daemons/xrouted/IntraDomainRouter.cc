//#include <signal.h>
#include <sys/time.h>
#include <math.h>

#include "../common/XIARouter.hh"
#include "IntraDomainRouter.hh"
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
// make the messages binary instead of text
// make the xiarouter return a map instead of vector


void *IntraDomainRouter::handler()
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

		struct pollfd pfd;

		pfd.fd = _sock;
		pfd.events = POLLIN;

		tspec.tv_sec = 0;
		tspec.tv_nsec = 500000000;

		rc = Xppoll(&pfd, 1, &tspec, NULL);
		if (rc > 0) {
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

			if ((rc = Xrecvmsg(_sock, &mh, 0)) < 0) {
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


int IntraDomainRouter::init()
{
	char dag[MAX_DAG_SIZE];

	// open socket for route process
	_sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (_sock < 0) {
		syslog(LOG_ALERT, "Unable to create a socket");
		exit(-1);
	}

	// make the broadcast dag we'll use to send to the other routers
	Graph g = Node() * Node(broadcast_fid) * Node(intradomain_sid);
	g.fill_sockaddr(&_ddag);
	syslog(LOG_INFO, "Router broadcast DAG: %s", g.dag_string().c_str());


	// get our AD and HID
	if ( XreadLocalHostAddr(_sock, dag, MAX_DAG_SIZE, NULL, 0) < 0 ) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		exit(-1);
	}
	Graph glocal(dag);
	_myAD = glocal.intent_AD_str();
	_myHID = glocal.intent_HID_str();

	// make the dag we will listen on
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, intradomain_sid.c_str(), NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&_sdag, ai->ai_addr, sizeof(sockaddr_x));
	Graph gg(&_sdag);
	syslog(LOG_INFO, "xroute Source DAG: %s", gg.dag_string().c_str());

	// bind to the src DAG
	if (Xbind(_sock, (struct sockaddr*)&_sdag, sizeof(sockaddr_x)) < 0) {
		Graph g(&_sdag);
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(_sock);
		exit(-1);
	}

	_num_neighbors = 0;				// number of seen neighbor routers
	_lsa_seq = rand() % MAX_SEQNUM;	// make our initial LSA sequence numbe

	_ctl_seq = 0;	// LSA sequence number of this router

	// FIXME: figure out what the correct type of router we are
	_flags = F_EDGE_ROUTER;

	if( XisDualStackRouter(_sock) == 1 ) {
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

int IntraDomainRouter::sendHello()
{
	int buflen, rc;
	string message;

	Node n_ad(_myAD);
	Node n_hid(_myHID);
	Node n_sid(intradomain_sid);

	Xroute::XrouteMsg msg;
	Xroute::HelloMsg *hello = msg.mutable_hello();
	Xroute::Node     *node  = hello->mutable_node();
	Xroute::XID      *ad    = node->mutable_ad();
	Xroute::XID      *hid   = node->mutable_hid();
	Xroute::XID      *sid   = node->mutable_sid();

	msg.set_type(Xroute::HELLO_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	hello->set_flags(_flags);
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);
	sid->set_type(n_sid.type());
	sid->set_id(n_sid.id(), XID_SIZE);


//	printf("sending %s\n", msg.DebugString().c_str());

	msg.SerializeToString(&message);
	buflen = message.length();

	rc = Xsendto(_sock, message.c_str(), buflen, 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));
	if (rc < 0) {
		// error!
		syslog(LOG_WARNING, "unable to send hello msg: %s", strerror(errno));

	} else if (rc != (int)message.length()) {
		syslog(LOG_WARNING, "ERROR sending hello. Tried sending %d bytes but rc=%d", buflen, rc);
		rc = -1;
	}

	return rc;
}

// send LinkStateAdvertisement message (flooding)
int IntraDomainRouter::sendLSA()
{
	int buflen, rc;
	string message;

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
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);

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

	msg.SerializeToString(&message);
	buflen = message.length();

	rc = Xsendto(_sock, message.c_str(), buflen, 0, (struct sockaddr*)&_ddag, sizeof(sockaddr_x));
	if (rc < 0) {
		// error!
		syslog(LOG_WARNING, "unable to send lsa msg: %s", strerror(errno));

	} else if (rc != (int)message.length()) {
		syslog(LOG_WARNING, "ERROR sending lsa. Tried sending %d bytes but rc=%d", buflen, rc);
		rc = -1;
	}

	return rc;
}

int IntraDomainRouter::processMsg(std::string msg_str, uint32_t iface)
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
			rc = processRoutingTable(msg.table_update());
			break;

		case Xroute::HOST_LEAVE_MSG:
			// FIXME: should this move to the controller?
			break;

		case Xroute::SID_TABLE_UPDATE_MSG:
			processSidRoutingTable(msg.sid_table_update());
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

int IntraDomainRouter::processSidRoutingTable(const Xroute::SIDTableUpdateMsg& msg)
{
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


	// FIXME: we shoudn't need this once flooding is turned on
	if (dstHID != _myHID) {
		std::string message;
		msg.SerializeToString(&message);
		uint32_t buflen = message.length();
		return Xsendto(_sock, message.c_str(), buflen, 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));
	}

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

int IntraDomainRouter::processHostRegister(const Xroute::HostJoinMsg& msg)
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

int IntraDomainRouter::processHello(const Xroute::HelloMsg &msg, uint32_t iface)
{
	string neighborAD, neighborHID, neighborSID;
	uint32_t flags = F_HOST;
	bool has_sid = msg.node().has_sid() ? true : false;

	Xroute::XID xad  = msg.node().ad();
	Xroute::XID xhid = msg.node().hid();

	Node  ad(xad.type(),  xad.id().c_str(), 0);
	Node hid(xhid.type(), xhid.id().c_str(), 0);
	Node sid;

	if (has_sid) {
		Xroute::XID xsid = msg.node().sid();
		sid = Node(xsid.type(), xsid.id().c_str(), 0);
		neighborSID = sid.to_string();
	}

	neighborAD  = ad. to_string();
	neighborHID = hid.to_string();

	if (msg.has_flags()) {
		flags = msg.flags();
	}

	// Update neighbor table
	NeighborEntry neighbor;
	neighbor.AD = neighborAD;
	if (!has_sid) {
		neighbor.HID = neighborHID;
	} else {
		neighbor.HID = neighborSID;
	}
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
	entry.num_neighbors = _num_neighbors;

	// Add neighbors to network table entry
	std::map<std::string, NeighborEntry>::iterator it;
	for (it = _neighborTable.begin(); it != _neighborTable.end(); it++)
		entry.neighbor_list.push_back(it->second);

	_networkTable[_myHID] = entry;

	syslog(LOG_INFO, "Process-Hello[%s]", neighbor.HID.c_str());

	return 1;
}

int IntraDomainRouter::processLSA(const Xroute::XrouteMsg& msg)
{
	// FIXME: we still need to reflood until the FID logic is switched out of broadcast mode

	string neighborAD, neighborHID;
	string srcAD, srcHID;
	int rc;

//	printf("processLSA: %s\n", lsa.DebugString().c_str());

	const Xroute::LSAMsg& lsa = msg.lsa();

	Xroute::XID a = lsa.node().ad();
	Xroute::XID h = lsa.node().hid();

	Node  ad(a.type(), a.id().c_str(), 0);
	Node hid(h.type(), h.id().c_str(), 0);

	srcAD  = ad.to_string();
	srcHID = hid.to_string();

//	syslog(LOG_INFO, "Process-LSA: %s %s", srcAD.c_str(), srcHID.c_str());

	if (srcHID.compare(_myHID) == 0) {
		// skip if from me
		return 1;
	}

	syslog(LOG_INFO, "Forward-LSA: %s %s", srcAD.c_str(), srcHID.c_str());

	std::string message;
	msg.SerializeToString(&message);
	uint32_t buflen = message.length();
	rc = Xsendto(_sock, message.c_str(), buflen, 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));

	return rc;
}


// process a control message
int IntraDomainRouter::processRoutingTable(const Xroute::TableUpdateMsg& msg)
{
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

	// FIXME: we shoudn't need this once flooding is turned on
	if ((dstAD != _myAD) || (dstHID != _myHID)) {
		std::string message;
		msg.SerializeToString(&message);
		uint32_t buflen = message.length();
		return Xsendto(_sock, message.c_str(), buflen, 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));
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

		_timeStamp[xid] = time(NULL);
	}
	return 1;
}
