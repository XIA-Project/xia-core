
#include <stdio.h>
#include <sys/time.h>
#include "dagaddr.hpp"
#include "minIni.h"
#include "Controller.hh"

// parameters which could be overwritten by controller file
// default values are defined in the header file
int expire_time = EXPIRE_TIME;
double HELLO_INTERVAL = HELLO_INTERVAL_D;
double LSA_INTERVAL = LSA_INTERVAL_D;
double SID_DISCOVERY_INTERVAL = SID_DISCOVERY_INTERVAL_D;
double SID_DECISION_INTERVAL = SID_DECISION_INTERVAL_D;
double AD_LSA_INTERVAL = AD_LSA_INTERVAL_D;
double CALC_DIJKSTRA_INTERVAL = CALC_DIJKSTRA_INTERVAL_D;
int UPDATE_LATENCY = UPDATE_LATENCY_D;
int UPDATE_CONFIG = UPDATE_CONFIG_D;
int MAX_HOP_COUNT  = MAX_HOP_COUNT_D;
int MAX_SEQNUM = MAX_SEQNUM_D;
int SEQNUM_WINDOW = SEQNUM_WINDOW_D;
int ENABLE_SID_CTL = ENABLE_SID_CTL_D;

void *Controller::handler()
{
	int rc;
	char recv_message[10240];
	sockaddr_x theirDAG;
	struct timespec tspec;
	vector<string> routers;
	time_t last_purge = time(NULL);
	time_t last_update_config = last_purge;
	time_t last_update_latency = last_purge;

	struct timeval now;

	gettimeofday(&now, NULL);
	if (timercmp(&now, &h_fire, >=)) {
		sendHello();
		timeradd(&now, &h_freq, &h_fire);
	}
	if (timercmp(&now, &l_fire, >=)) {
		sendInterDomainLSA();
		timeradd(&now, &l_freq, &l_fire);
	}
	if (timercmp(&now, &sd_fire, >=)) {
//		sendSidDiscovery();
		timeradd(&now, &sd_freq, &sd_fire);
	}
	if (timercmp(&now, &sq_fire, >=)) {
		if (ENABLE_SID_CTL) {
//			querySidDecision();
		}
		timeradd(&now, &sq_freq, &sq_fire);
	}

	struct pollfd pfd[2];

	pfd[0].fd = _rsock;
	pfd[0].events = POLLIN;
	pfd[1].fd = _csock;
	pfd[1].events = POLLIN;

	tspec.tv_sec = 0;
	tspec.tv_nsec = 500000000;

	rc = Xppoll(pfd, 2, &tspec, NULL);
	if (rc > 0) {
		int sock;

		if (pfd[0].revents & POLLIN) {
			sock = pfd[0].fd;
		} else if (pfd[1].revents & POLLIN) {
			sock = pfd[1].fd;
		} else {
			// something bad may have happened
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

	if (nowt -last_update_config >= UPDATE_CONFIG)
	{
		last_update_config = nowt;
		set_controller_conf(_hostname);
		set_sid_conf(_hostname);

	}

	if (nowt - last_update_latency >= UPDATE_LATENCY)
	{
		last_update_latency = nowt;
		updateADPathStates(); // update latency info
	}

	if (nowt - last_purge >= expire_time)
	{
		last_purge = nowt;
		//fprintf(stderr, "checking entry\n");
		map<string, time_t>::iterator iter = _timeStamp.begin();

		while (iter != _timeStamp.end())
		{
			if (nowt - iter->second >= expire_time*10) {
				_xr.delRoute(iter->first);
				last_update_latency = 0; // force update latency
				syslog(LOG_INFO, "purging host route for : %s", iter->first.c_str());
				_timeStamp.erase(iter++);
			} else {
				++iter;
			}
		}

		map<string, NodeStateEntry>::iterator iter1 = _ADNetworkTable.begin();

		while (iter1 != _ADNetworkTable.end())
		{
			if (nowt - iter1->second.timestamp >= expire_time*10) {
				syslog(LOG_INFO, "purging neighbor : %s", iter1->first.c_str());
				last_update_latency = 0; // force update latency
				_ADNetworkTable.erase(iter1++);
			} else {
				++iter1;
			}
		}

		map<string, NeighborEntry>::iterator iter2 = _neighborTable.begin();

		while (iter2 != _neighborTable.end())
		{
			if (nowt - iter2->second.timestamp >= expire_time) {
				last_update_latency = 0; // force update latency
				syslog(LOG_INFO, "purging AD network : %s", iter2->first.c_str());
				_neighborTable.erase(iter2++);
				_num_neighbors -= 1;
			} else {
				++iter2;
			}
		}

		map<string, NeighborEntry>::iterator iter3 = _ADNeighborTable.begin();

		while (iter3 != _ADNeighborTable.end())
		{
			if (nowt - iter3->second.timestamp >= expire_time*10) {
				last_update_latency = 0; // force update latency
				syslog(LOG_INFO, "purging AD neighbor : %s", iter3->first.c_str());

				_ADNetworkTable[_myAD].neighbor_list.erase(
					std::remove(_ADNetworkTable[_myAD].neighbor_list.begin(),
								_ADNetworkTable[_myAD].neighbor_list.end(),
								iter3->second),
				_ADNetworkTable[_myAD].neighbor_list.end());
				_ADNeighborTable.erase(iter3++);
			} else {
				++iter3;
			}
		}
	}

	return NULL;
}

int Controller::init()
{
	char dag[MAX_DAG_SIZE];

	srand (time(NULL));

	_flags = F_CONTROLLER; // FIXME: set any other useful flags at this time

	// open socket for route process
	_rsock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (_rsock < 0) {
		syslog(LOG_ALERT, "Unable to create a socket");
		exit(-1);
	}

	// make the broadcast dag we'll use to send to our child routers
	Graph g = Node() * Node(broadcast_fid) * Node(intradomain_sid);
	g.fill_sockaddr(&_ddag);
	syslog(LOG_INFO, "Controller broadcast DAG: %s", g.dag_string().c_str());

	// get our AD and HID
	if ( XreadLocalHostAddr(_rsock, dag, MAX_DAG_SIZE, NULL, 0) < 0 ) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		exit(-1);
	}
	Graph glocal(dag);
	_myAD = glocal.intent_AD_str();
	_myHID = glocal.intent_HID_str();

	// make the src DAG (the one we talk to child routers with)
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, intradomain_sid.c_str(), NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&_sdag, ai->ai_addr, sizeof(sockaddr_x));
	Graph gg(&_sdag);
	syslog(LOG_INFO, "xroute Source DAG: %s", gg.dag_string().c_str());

	// bind to the src DAG
	if (Xbind(_rsock, (struct sockaddr*)&_sdag, sizeof(sockaddr_x)) < 0) {
		Graph g(&_sdag);
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(_rsock);
		exit(-1);
	}

	// now make the contoller socket
	_csock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (_csock < 0) {
		syslog(LOG_ALERT, "Unable to create a socket");
		exit(-1);
	}

	// bind to the controller service
	sockaddr_x tempDAG;
	if (Xgetaddrinfo(NULL, controller_sid.c_str(), NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to bind controller service");
		exit(-1);
	}
	memcpy(&tempDAG, ai->ai_addr, sizeof(sockaddr_x));

	if (Xbind(_csock, (struct sockaddr*)&tempDAG, sizeof(sockaddr_x)) < 0) {
		Graph g(&tempDAG);
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(_csock);
		exit(-1);
	}


	_num_neighbors = 0; // number of neighbor routers
	_lsa_seq = rand() % MAX_SEQNUM;	// LSA sequence number of this router
	_sid_discovery_seq = rand()%MAX_SEQNUM;  // sid discovery seq number of this router
	_calc_dijstra_ticks = -8;

	_ctl_seq = 0;	// LSA sequence number of this router
	_sid_ctl_seq = 0; // TODO: init values should be a random int

	_dual_router_AD = "NULL";
	// mark if this is a dual XIA-IPv4 router
	if( XisDualStackRouter(_rsock) == 1 ) {
		_dual_router = 1;
		syslog(LOG_DEBUG, "configured as a dual-stack router");
	} else {
		_dual_router = 0;
	}

	struct timeval now;

	h_freq.tv_sec = 0;
	h_freq.tv_usec = 100000;
	l_freq.tv_sec = 0;
	l_freq.tv_usec = 300000;
	sd_freq.tv_sec = 3;
	sd_freq.tv_usec = 0;
	sq_freq.tv_sec = 5;
	sq_freq.tv_usec = 0;

	gettimeofday(&now, NULL);
	timeradd(&now, &h_freq, &h_fire);
	timeradd(&now, &l_freq, &l_fire);

	return 0;
}

// FIXME: this did 2 sends in the original SDN code, will this change work?
int Controller::sendHello()
{
	int buflen, rc;
	string message;

	Node n_ad(_myAD);
	Node n_hid(_myHID);
	Node n_sid(controller_sid);

	Xroute::XrouteMsg msg;
	Xroute::HelloMsg *hello = msg.mutable_hello();
	Xroute::Node     *node  = hello->mutable_node();
	Xroute::XID      *ad    = node->mutable_ad();
	Xroute::XID      *hid   = node->mutable_hid();
	Xroute::XID      *sid   = node->mutable_sid();

	msg.set_type(Xroute::HELLO_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	hello->set_flags(F_CONTROLLER);
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);
	sid->set_type(n_sid.type());
	sid->set_id(n_sid.id(), XID_SIZE);


//	printf("sending %s\n", msg.DebugString().c_str());

	msg.SerializeToString(&message);
	buflen = message.length();

	rc = Xsendto(_rsock, message.c_str(), buflen, 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));
	if (rc < 0) {
		// error!
		syslog(LOG_WARNING, "unable to send hello msg: %s", strerror(errno));

	} else if (rc != (int)message.length()) {
		syslog(LOG_WARNING, "ERROR sending hello. Tried sending %d bytes but rc=%d", buflen, rc);
		rc = -1;
	}

	return rc;
}

int Controller::sendInterDomainLSA()
{
	int rc = 1;
	//syslog(LOG_INFO, "Controller::Send-LSA[%d]", _lsa_seq);

	Node a(_myAD);
	Node h(_myHID);	// FIXME: the original code uses the AD here
					// and never does anything with it. Making it HID for nwo

	Xroute::XrouteMsg msg;
	Xroute::GlobalLSAMsg *lsa  = msg.mutable_global_lsa();
	Xroute::Node         *from = lsa->mutable_from();
	Xroute::XID          *ad   = from->mutable_ad();
	Xroute::XID          *hid  = from->mutable_hid();

	msg.set_type(Xroute::GLOBAL_LSA_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	ad ->set_type(a.type());
	ad ->set_id(a.id(), XID_SIZE);
	hid->set_type(h.type());
	hid->set_id(h.id(), XID_SIZE);
	lsa->set_flags(_flags);

	std::map<std::string, NeighborEntry>::iterator it;
	for (it = _ADNeighborTable.begin(); it != _ADNeighborTable.end(); it++)
	{
		Node aa(it->second.AD);
		Node hh(it->second.HID);

		Xroute::NeighborEntry *n = lsa->add_neighbors();
		ad  = n->mutable_ad();
		hid = n->mutable_hid();

		ad ->set_type(aa.type());
		ad ->set_id(aa.id(), XID_SIZE);
		hid->set_type(hh.type());
		hid->set_id(hh.id(), XID_SIZE);

		n->set_port(it->second.port);
		n->set_cost(it->second.cost);
	}

	for (it = _ADNeighborTable.begin(); it != _ADNeighborTable.end(); it++)
	{
		string message;
		sockaddr_x ddag;
		Graph g = Node() * Node(it->second.AD) * Node(controller_sid);
		g.fill_sockaddr(&ddag);

		//syslog(LOG_INFO, "send inter-AD LSA[%d] to %s", _lsa_seq, it->second.AD.c_str());
		//syslog(LOG_INFO, "msg: %s", msg.c_str());

		msg.SerializeToString(&message);

		int temprc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));
		if (temprc < 0) {
			// error!
			syslog(LOG_ERR, "error sending inter-AD LSA to %s", it->second.AD.c_str());

		} else if (temprc != (int)message.length()) {
			syslog(LOG_WARNING, "ERROR sending inter-AD LSA. Tried sending %lu bytes but rc=%d", message.length(), temprc);
			temprc = -1;
		}
		rc = (temprc < rc)? temprc : rc;
	}

	return rc;
}

int Controller::sendRoutingTable(std::string destHID, std::map<std::string, RouteEntry> routingTable)
{
	syslog(LOG_INFO, "Controller::Send-RT");

	if (destHID == _myHID) {
		// If destHID is self, process immediately
		return processRoutingTable(routingTable);
	} else {
		// If destHID is not SID, send to relevant router
		Node fad(_myAD);
		Node fhid(_myHID);
		Node tad(_myAD);
		Node thid(destHID);

		Xroute::XrouteMsg msg;
		Xroute::TableUpdateMsg *t    = msg  .mutable_table_update();
		Xroute::Node           *from = t   ->mutable_from();
		Xroute::Node           *to   = t   ->mutable_from();
		Xroute::XID            *fa   = from->mutable_ad();
		Xroute::XID            *fh   = from->mutable_hid();
		Xroute::XID            *ta   = to  ->mutable_ad();
		Xroute::XID            *th   = to  ->mutable_hid();

		msg.set_type(Xroute::TABLE_UPDATE_MSG);
		msg.set_version(Xroute::XROUTE_PROTO_VERSION);
		fa ->set_type(fad.type());
		fa ->set_id(fad.id(), XID_SIZE);
		fh ->set_type(fhid.type());
		fh ->set_id(fhid.id(), XID_SIZE);
		ta ->set_type(tad.type());
		ta ->set_id(tad.id(), XID_SIZE);
		th ->set_type(thid.type());
		th ->set_id(thid.id(), XID_SIZE);

		map<string, RouteEntry>::iterator it;
		for (it = routingTable.begin(); it != routingTable.end(); it++)
		{
			Xroute::TableEntry *e = t->add_routes();
			Node dest(it->second.dest);
			Node nexthop(it->second.nextHop);
			Xroute::XID *x = e->mutable_xid();

			x->set_type(dest.type());
			x->set_id(dest.id(), XID_SIZE);

			x = e->mutable_next_hop();
			x->set_type(nexthop.type());
			x->set_id(nexthop.id(), XID_SIZE);

			e->set_port(it->second.port);
			e->set_flags(it->second.flags);
		}

		// FIXME: use the FID and SID from the router
		string message;
		sockaddr_x ddag;
		Graph g = Node() * Node(broadcast_fid) * Node(intradomain_sid);
		g.fill_sockaddr(&ddag);

		//syslog(LOG_INFO, "send inter-AD LSA[%d] to %s", _lsa_seq, it->second.AD.c_str());
		//syslog(LOG_INFO, "msg: %s", msg.c_str());

		msg.SerializeToString(&message);

		int rc = Xsendto(_rsock, message.c_str(), message.length(), 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));
		if (rc < 0) {
			// error!
			syslog(LOG_ERR, "error sending Table Update Msg to %s", it->second.dest.c_str());
		}

		return rc;
	}
}

#if 0
int Controller::sendSidDiscovery()
{
	int rc = 1;

	syslog(LOG_INFO, "Controller::Send-SID Disc");

	Node ad(_myAD);
	Node hid(_myHID);

	Xroute::XrouteMsg msg;
	Xroute::SIDDiscoveryMsg *d = msg.mutable_sid_discovery();
	Xroute::XID             *a = d  ->mutable_ad();
	Xroute::XID             *h = d  ->mutable_hid();

	a->set_type(ad.type());
	a->set_id(ad.id(), XID_SIZE);
	h->set_type(hid.type());		// FIXME: original code used the ad over again
	h->set_id(hid.id(), XID_SIZE);	//  and once again doesn't use the hid field at all

	//broadcast local services
	if (_LocalSidList.empty() && _SIDADsTable.empty())
	{
		// Nothing to send
		// syslog(LOG_INFO, "%s LocalSidList is empty, nothing to send", _myAD);
		return rc;

	} else {
		// prepare the packet TODO: only send updated entries TODO: but don't forget to renew TTL
		sendKeepAliveToServiceControllerLeader(); // temporally put it here
		// TODO: the format of this packet could be reduced much
		// local info
		std::map<std::string, ServiceState>::iterator it;
		for (it = _LocalSidList.begin(); it != _LocalSidList.end(); ++it)
		{
			Node s(it->first);

			Xroute::DiscoveryEntry *e = d->add_entries();

			Xroute::XID *x = e->mutable_ad();
			x->set_type(ad.type());
			x->set_id(ad.id(), XID_SIZE);

			x = e->mutable_sid();
			x->set_type(s.type());
			x->set_id(s.id(), XID_SIZE);

			// TODO: append more attributes/parameters
			e->set_capacity(it->second.capacity);
			e->set_capacity_factor(it->second.capacity_factor);
			e->set_link_factor(it->second.link_factor);
			e->set_priority(it->second.priority);
			e->set_leader_addr(it->second.leaderAddr);
			e->set_arch_type(it->second.archType);
			e->set_seq(it->second.seq);
		}

		// rebroadcast services learnt from others
		std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
		for (it_sid = _SIDADsTable.begin(); it_sid != _SIDADsTable.end(); ++it_sid)
		{
			std::map<std::string, ServiceState>::iterator it_ad;
			for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
			{
				Node aa(it_ad->first);
				Node ss(it_sid->first);

				Xroute::DiscoveryEntry *e = d->add_entries();

				Xroute::XID *x = e->mutable_ad();
				x->set_type(aa.type());
				x->set_id(aa.id(), XID_SIZE);

				x = e->mutable_sid();
				x->set_type(ss.type());
				x->set_id(ss.id(), XID_SIZE);

				// TODO: put information from service_state inside
				e->set_capacity(it_ad->second.capacity);
				e->set_capacity_factor(it_ad->second.capacity_factor);
				e->set_link_factor(it_ad->second.link_factor);
				e->set_priority(it_ad->second.priority);
				e->set_leader_addr(it_ad->second.leaderAddr);
				e->set_arch_type(it_ad->second.archType);
				e->set_seq(it_ad->second.seq);
			}
		}
	}

	// send it to neighbor ADs
	// TODO: reuse code: broadcastToADNeighbors(msg)?

	// locallly process first
	processSidDiscovery(msg.sid_discovery());

	std::map<std::string, NeighborEntry>::iterator it;

	for (it = _ADNeighborTable.begin(); it != _ADNeighborTable.end(); ++it)
	{
		sockaddr_x ddag;
		Graph g = Node() * Node(it->second.AD) * Node(controller_sid);
		g.fill_sockaddr(&ddag);

		string message;
		msg.	SerializeToString(&message);

		//syslog(LOG_INFO, "send inter-AD LSA[%d] to %s", _lsa_seq, it->second.AD.c_str());
		//syslog(LOG_INFO, "msg: %s", msg.c_str());
		int temprc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));
		if (temprc < 0) {
			syslog(LOG_ERR, "error sending inter-AD SID discovery to %s", it->second.AD.c_str());
		}
		rc = (temprc < rc)? temprc : rc;
	}

	return rc;
}
#endif

int Controller::processInterdomainLSA(const Xroute::GlobalLSAMsg& msg)
{
	NodeStateEntry entry;

	Xroute::XID xad  = msg.from().ad();
	Xroute::XID xhid = msg.from().hid();
	//Xroute::XID xsid = msg.node().sid();

	entry.ad  = Node(xad.type(),  xad.id().c_str(),  0).to_string();
	entry.hid = Node(xhid.type(), xhid.id().c_str(), 0).to_string();
	//string srcSID = Node(xsid.type(), xsid.id().c_str(), 0).to_string();

	// First, filter out the LSA originating from myself
	if (entry.ad == _myAD) {
		return 1;
	}

	entry.num_neighbors = msg.neighbors_size();

	// See if this LSA comes from AD with dualRouter
	if (msg.flags() & F_IP_GATEWAY) {
		_dual_router_AD = entry.ad;
	}

	//syslog(LOG_INFO, "inter-AD LSA from %s, %d neighbors", entry.ad.c_str(), numNeighbors);
	entry.timestamp = time(NULL);

	for (uint32_t i = 0; i < entry.num_neighbors; i++) {
		NeighborEntry neighbor;

		Xroute::NeighborEntry n = msg.neighbors(i);

		xad  = n.ad();
		xhid = n.hid();

		neighbor.AD  = Node(xad.type(),  xad.id().c_str(),  0).to_string();
		neighbor.HID = Node(xhid.type(), xhid.id().c_str(), 0).to_string();

		neighbor.port = n.port();
		neighbor.cost = n.cost();

		//syslog(LOG_INFO, "neighbor[%d] = %s", i, neighbor.AD.c_str());

		entry.neighbor_list.push_back(neighbor);
	}

	_ADNetworkTable[entry.ad] = entry;

	// Rebroadcast this LSA
	int rc = 1;
	std::map<std::string, NeighborEntry>::iterator it;
	for (it = _ADNeighborTable.begin(); it != _ADNeighborTable.end(); it++) {
		sockaddr_x ddag;
		Graph g = Node() * Node(it->second.AD) * Node(controller_sid);
		g.fill_sockaddr(&ddag);

		string message;
		msg.SerializeToString(&message);
		int temprc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&ddag, sizeof(sockaddr_x));
		rc = (temprc < rc)? temprc : rc;
	}
	return rc;
}

int Controller::sendKeepAliveToServiceControllerLeader()
{
	int rc = 1;

	std::map<std::string, ServiceState>::iterator it;
	for (it = _LocalSidList.begin(); it != _LocalSidList.end(); ++it)
	{
		Node sid(it->first);
		Node  ad(myAD);
		Node hid(myHID);

		Xroute::XrouteMsg msg;
		Xroute::KeepAliveMsg *ka = msg.mutable_keep_alive();
		Xroute::Node         *n  = ka->mutable_from();
		Xroute::XID          *a  = n ->mutable_ad();
		Xroute::XID          *h  = n ->mutable_hid();
		Xroute::XID          *s  = ka->mutable_sid();

		msg.set_type(Xroute::SID_MANAGE_KA_MSG);
		msg.set_version(Xroute::XROUTE_PROTO_VERSION);
		a->set_type(ad.type());
		a->set_id(ad.id(), XID_SIZE);
		h->set_type(hid.type());
		h->set_id(hid.id(), XID_SIZE);
		s->set_type(sid.type());
		s->set_id(sid.id(), XID_SIZE);
		ka->set_capacity(it->second.capacity);
		ka->set_delay(it->second.internal_delay);

		// FIXME: there is a lot more in the servicestate struct than is being used here

		if (it->second.isLeader) {
			processServiceKeepAlive(msg.keep_alive()); // process locally

		} else {
			sockaddr_x ddag;
			string message;

			xia_pton(AF_XIA, it->second.leaderAddr.c_str(), &ddag);
			msg.SerializeToString(&message);
			rc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&ddag, sizeof(sockaddr_x));
			//syslog(LOG_DEBUG, "sent SID %s keep alive to %s", it->first.c_str(), it->second.leaderAddr.c_str());
		}
	}

	return rc;
}

int Controller::processServiceKeepAlive(const Xroute::KeepAliveMsg &msg)
{
	int rc = 1;
	string neighborAD, neighborHID, sid;

	Xroute::XID xad  = msg.from().ad();
	Xroute::XID xhid = msg.from().hid();
	Xroute::XID xsid = msg.sid();

	neighborAD  = Node(xad.type(),  xad.id().c_str(),  0).to_string();
	neighborHID = Node(xhid.type(), xhid.id().c_str(), 0).to_string();
	sid         = Node(xsid.type(), xsid.id().c_str(), 0).to_string();

	uint32_t capacity = msg.capacity();
	uint32_t delay    = msg.delay();


	if (_LocalServiceLeaders.find(sid) != _LocalServiceLeaders.end()) {
		// if controller is here
		//update attributes
		_LocalServiceLeaders[sid].instances[neighborAD].capacity = capacity;
		_LocalServiceLeaders[sid].instances[neighborAD].internal_delay = delay;
		// TODO: reply to that instance?
		//syslog(LOG_DEBUG, "got SID %s keep alive from %s", sid.c_str(), srcAddr.c_str());

	} else {
		syslog(LOG_ERR, "got %s keep alive to its service leader from %s, but I am not its leader!", sid.c_str(), neighborAD.c_str());
		rc = -1;
	}

	return rc;
}

#if 0
int Controller::processSidDiscovery(const Xroute::SIDDiscoveryMsg& msg)
{
	//TODO: add version(time-stamp?) for each entry
	int rc = 1;

	Xroute::XID a = msg.ad();
	Xroute::XID h = msg.hid();
	string srcAD  = Node(a.type(), a.id().c_str(), 0).to_string();
	string srcHID = Node(h.type(), h.id().c_str(), 0).to_string();

	//syslog(LOG_INFO, "Get SID discovery msg from %s", srcAD.c_str());

	// process the entries: AD-SID pairs
	//syslog(LOG_INFO, "Get %d origin SIDs", records);
	for (int i = 0; i < msg.entries_size(); ++i)
	{
		ServiceState service_state;
		Xroute::DiscoveryEntry d = msg.entries(i);

		Xroute::XID x = d.ad();
		string AD = Node(x.type(), x.id().c_str(), 0).to_string();

		x = d.sid();
		string SID = Node(x.type(), x.id().c_str(), 0).to_string();

		//TODO: read the parameters from msg, put them into service_state
		service_state.capacity = d.capacity();
		service_state.capacity_factor = d.capacity_factor();
		service_state.link_factor = d.link_factor();
		service_state.priority = d.priority();
		//syslog(LOG_INFO, "Get broadcast SID %s@%s, p= %d,s=%d",SID.c_str(), AD.c_str(), priority, seq);
		service_state.leaderAddr = d.leader_addr();
		service_state.archType = d.arch_type();
		service_state.seq = d.seq();
		service_state.percentage = 0;
		updateSidAdsTable(AD, SID, service_state);
	}

	//syslog(LOG_INFO, "SID-ADs %lu", _SIDADsTable.size());
	// rc = processSidDecision(); // use a timeout callback instead?
	// TODO: read the config file frequently, emulate service failure
	// TODO: how/when to revoke/remove a entry
	// TODO: TTL for AD-SID pair for fast failure discovery/recovery
	return rc;
}

int Controller::querySidDecision()
{
	// New design. Report local information to sid controller. Then wait for answer.
	//syslog(LOG_DEBUG, "Sending SID decision queries for %lu SIDs", _SIDADsTable.size() );
	int rc = 1;
	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;

	for (it_sid = _SIDADsTable.begin(); it_sid != _SIDADsTable.end(); ++it_sid)
	{
		// for each SID, generate a report packet.
		// packet format: SID/rate/#options/optionAD1/Latency1,capacity1/optionAD2/Latency2...

		//syslmog(LOG_DEBUG, "Sending SID decision queries for %s", it_sid->first.c_str() );

		Node ad(_myAD);
		Node hid(_myHID);
		Node sid(it_sid->first);

		Xroute::XrouteMsg msg;
		Xroute::DecisionQueryMsg *q = msg.mutable_sid_query();
		Xroute::XID              *a  = q ->mutable_ad();
		Xroute::XID              *h  = q ->mutable_hid();
		Xroute::XID              *s  = q ->mutable_sid();

		msg.set_type(Xroute::SID_DECISION_QUERY_MSG);
		msg.set_version(Xroute::XROUTE_PROTO_VERSION);
		a->set_type(ad.type());
		a->set_id(ad.id(), XID_SIZE);
		h->set_type(hid.type());
		h->set_id(hid.id(), XID_SIZE);
		s->set_type(sid.type());
		s->set_id(sid.id(), XID_SIZE);


		int rate = 0;
		if (_SIDRateTable.find(it_sid->first) != _SIDRateTable.end()) {
			rate = _SIDRateTable[it_sid->first];
		}
		if (rate < 0) { // it should not happen
			rate = 0;
		}

		q->set_rate(rate);

		std::map<std::string, ServiceState>::iterator it_ad;
		std::string best_ad; // the cloest controller
		int minimal_latency = 9999; // smallest latency

		for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
		{ // second pass, find the cloest one, creat the message
			if (it_ad->second.priority < 0) { // this is the poisoned one, skip
				continue;
			}
			int latency = 9998; // default, 9999-1ms
			if (_ADPathStates.find(it_ad->first) != _ADPathStates.end()) {
				latency = _ADPathStates[it_ad->first].delay;
			}
			minimal_latency = minimal_latency > latency?latency:minimal_latency;
			best_ad = minimal_latency == latency?it_ad->first:best_ad; // find the cloest

			Node aa(it_ad->first);

			Xroute::QueryEntry *e = q->add_ads();
			a = e->mutable_ad();
			a->set_type(aa.type());
			a->set_id(aa.id(), XID_SIZE);
			e->set_latency(latency);
			e->set_capacity(it_ad->second.capacity); //capacity
		}
		//syslog(LOG_DEBUG, "Going to send to %s", best_ad.c_str() );

		// send the msg
		if (best_ad == _myAD) { // I'm the one
			//syslog(LOG_DEBUG, "Sending SID decision query locally");
			processSidDecisionQuery(msg); // process locally

		} else {

			string message;
			sockaddr_x ddag;
			Graph g = Node() * Node(best_ad) * Node(controller_sid);
			g.fill_sockaddr(&ddag);

			msg.SerializeToString(&message);
			int temprc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&ddag, sizeof(sockaddr_x));
			if (temprc < 0) {
				syslog(LOG_ERR, "error sending SID decision query to %s", best_ad.c_str());
			}
			rc = (temprc < rc)? temprc : rc;
			//syslog(LOG_DEBUG, "sent SID %s decision query to %s", it_sid->first.c_str(), best_ad.c_str());
		}
	}
	return rc;
}

int Controller::processSidDecisionQuery(const Xroute::XrouteMsg& msg)
{
	// work out a decision for each query
	// TODO: This could/should be async
	//syslog(LOG_DEBUG, "Processing SID decision query");
	int rc = 1;

	Xroute::XID xad  = msg.sid_query().ad();
	Xroute::XID xhid = msg.sid_query().hid();
	Xroute::XID xsid = msg.sid_query().sid();

	string srcAD  = Node(xad.type(),  xad.id().c_str(),  0).to_string();
	string srcHID = Node(xhid.type(), xhid.id().c_str(), 0).to_string();
	string SID    = Node(xsid.type(), xsid.id().c_str(), 0).to_string();

	uint32_t rate = msg.sid_query().rate();

	//syslog(LOG_INFO, "Get %s query msg from %s, rate %d",SID.c_str(), srcAD.c_str(), rate);

	// process the entries: AD-latency pairs
	// check if I am the SID controller
	std::map<std::string, ServiceState>::iterator it_sid;
	it_sid = _LocalSidList.find(SID);
	if (it_sid == _LocalSidList.end()) {
		// not found, I'm not the right controller to talk to
		syslog(LOG_INFO, "I'm not the controller for %s", SID.c_str());
		// TODO: reply error msg to the source - return???
	}

	if (it_sid->second.archType == ARCH_CENT && !it_sid->second.isLeader) { // should forward to the leader

		// are we forging src addr?
		sockaddr_x ddag;
		string message;
		//syslog(LOG_INFO, "Send forward query %s", it_sid->second.leaderAddr.c_str());
		Graph g(it_sid->second.leaderAddr);
		g.fill_sockaddr(&ddag);

		msg.SerializeToString(&message);
		rc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&ddag, sizeof(sockaddr_x));
		if (rc < 0) {
			syslog(LOG_ERR, "error sending SID query forwarding to %s", it_sid->second.leaderAddr.c_str());
		}
		return rc;

	} else {
		std::map<std::string, DecisionIO> decisions;
		for (int i = 0; i < msg.sid_query().ads_size(); ++i) {
			DecisionIO dio;

			Xroute::QueryEntry q = msg.sid_query().ads(i);
			Xroute::XID a = q.ad();

			string ad = Node(a.type(), a.id().c_str(), 0).to_string();

			dio.capacity   = q.capacity();
			dio.latency    = q.latency();
			dio.percentage = 0;
			decisions[ad] = dio;
			//syslog(LOG_INFO, "Get %d ms for %s", latency, AD.c_str());
		}
		// compute the weights
		it_sid->second.decision(SID, srcAD, rate, &decisions);

		//reply to the query
		Node aa(_myAD);
		Node hh(_myHID);

		Xroute::XID *x;
		Xroute::XrouteMsg xm;
		Xroute::DecisionAnswerMsg *dam = xm.mutable_sid_answer();

		xm.set_type(Xroute::SID_DECISION_ANSWER_MSG);
		xm.set_version(Xroute::XROUTE_PROTO_VERSION);


		x = dam->mutable_ad();
		x->set_type(aa.type());
		x->set_id(aa.id(), XID_SIZE);

		x = dam->mutable_hid();
		x->set_type(hh.type());
		x->set_id(hh.id(), XID_SIZE);

		x = dam->mutable_sid();
		x->set_type(xsid.type());
		x->set_id(xsid.id().c_str(), XID_SIZE);

		std::map<std::string, DecisionIO>::iterator it_ds;
		for (it_ds = decisions.begin(); it_ds != decisions.end(); ++it_ds) {
			Xroute::QueryAnswer *qa = dam->add_sids();

			Node xx(it_ds->first);
			Xroute::XID *xa = qa->mutable_ad();
			xa->set_type(xx.type());
			xa->set_id(xx.id(), XID_SIZE);

			qa->set_percentage(it_ds->second.percentage);
		}

		// send the msg
		if (srcAD == _myAD) { // I'm the one
			//syslog(LOG_DEBUG, "Sending SID decision locally");
			processSidDecisionAnswer(xm.sid_answer()); // process locally
		} else {
			string message;
			sockaddr_x ddag;
			Graph g = Node() * Node(srcAD) * Node(controller_sid);
			g.fill_sockaddr(&ddag);

			msg.SerializeToString(&message);
			rc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&ddag, sizeof(sockaddr_x));
			if (rc < 0) {
				syslog(LOG_ERR, "error sending SID decision answer to %s", srcAD.c_str());
			}
			//syslog(LOG_DEBUG, "sent SID %s decision answer to %s", SID.c_str(), srcAD.c_str());
		}

		return rc;
	}
}

int Controller::Latency_first(std::string SID, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision)
{
	//syslog(LOG_DEBUG, "Decision function %s: Latency_first for %s", SID.c_str(), srcAD.c_str());
	if (srcAD == "" || rate < 0) {
		syslog(LOG_INFO, "Error parameters");
		return -1;
	}
	string best_ad;
	std::map<std::string, DecisionIO>::iterator it_ad;
	int minimal_latency = 9999; // smallest latency

	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // first pass, just find the minimal latency
		// e2e latency + internal delay
		int delay = it_ad->second.latency;
		if ( _LocalServiceLeaders[SID].instances.find(it_ad->first) != _LocalServiceLeaders[SID].instances.end()) {
			delay += _LocalServiceLeaders[SID].instances[it_ad->first].internal_delay;
		}
		minimal_latency = minimal_latency > delay?delay:minimal_latency;
		best_ad = minimal_latency == delay?it_ad->first:best_ad; // find the closest
	}

	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // second pass, assign weight
		if (it_ad->first ==  best_ad) {
			it_ad->second.percentage = 100;
		} else {
			it_ad->second.percentage = 0;
		}
	}
	return 0;
}

int Controller::Load_balance(std::string, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision)
{
	//syslog(LOG_DEBUG, "Decision function %s: load balance for %s", SID.c_str(), srcAD.c_str());
	if (srcAD == "" || rate < 0) {
		syslog(LOG_INFO, "Error parameters");
		return -1;
	}
	if (decision == NULL || decision->empty()) {
		syslog(LOG_INFO, "Get 0 entries to decide!");
		return -1;
	}
	std::map<std::string, DecisionIO>::iterator it_ad;
	int weight = 0;

	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // first pass, count capacity
		weight += it_ad->second.capacity;
	}
	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // second pass, assign weight
		it_ad->second.percentage = 100.0 * it_ad->second.capacity / weight + 0.5; //round
	}

	return 0;
}

bool Controller::compareCL(const ClientLatency &a, const ClientLatency &b)
{
	return a.latency > b.latency;
}


int Controller::Rate_load_balance(std::string SID, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision)
{ // balance the load depending on the capacity of replicas and traffic rate of client domains
	if (srcAD == "" || rate < 0) {
		syslog(LOG_INFO, "Error parameters");
		return -1;
	}
	if (decision == NULL || decision->empty()) {
		syslog(LOG_INFO, "Get 0 entries to decide!");
		return -1;
	}
	//update rate first
	_LocalServiceLeaders[SID].rates[srcAD] = rate;
	std::map<std::string, int> rates = _LocalServiceLeaders[SID].rates;
	std::map<std::string, int>::iterator dumpit;

	/*mark the 0 rate client as -1*/
	for (dumpit = rates.begin(); dumpit != rates.end(); ++dumpit) {
		if (dumpit->second == 0) {
			dumpit->second = -1;
		}
	}

	std::map<std::string, int> capacities;

	// update latency
	std::map<std::string, DecisionIO>::iterator it_ad;
	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // update latency
		_LocalServiceLeaders[SID].latencies[it_ad->first][srcAD] = it_ad->second.latency;
		capacities[it_ad->first] = it_ad->second.capacity;
		//syslog(LOG_DEBUG, "record capacity %s %d", it_ad->first.c_str(), it_ad->second.capacity);
	}

	// compute the weight

	//first, convert to lists sorted by latency
	std::map<std::string, std::vector<ClientLatency> > latency_map;
	std::map<std::string, std::map<std::string, int> >::iterator it = _LocalServiceLeaders[SID].latencies.begin();
	for (; it != _LocalServiceLeaders[SID].latencies.end(); ++it) {
		//syslog(LOG_DEBUG, "convert %s", it->first.c_str());

		std::vector<ClientLatency> cls;
		std::map<std::string, int>::iterator it2 = it->second.begin();
		for ( ; it2 != it->second.end(); ++it2) {
			//syslog(LOG_DEBUG, "convert2 %s %d", it2->first.c_str(), it2->second);
			ClientLatency cl;
			cl.AD = it2->first;
			cl.latency = it2->second;
			cls.push_back(cl);
		}
		std::sort(cls.begin(), cls.end(), compareCL);
		latency_map[it->first] = cls;
	}

	//second, heuristic allocation, allocate full capacity to the client-replica pair with the smallest latency, then repeat.

	std::map<std::string, std::vector<ClientLatency> >::iterator it_lp;
	std::map<std::string, std::vector<ClientLatency> >::iterator it_best;
	while (1) {
		/*find the pair*/
		it_best = latency_map.end();
		std::string AD;
		for (it_lp = latency_map.begin(); it_lp != latency_map.end(); ++it_lp) {
			/*clean up*/
			if (it_lp->second.empty()) { // this list is empty
				continue;
			}
			if (capacities[it_lp->first] <= 0) {
				while (!it_lp->second.empty()) { // no more capacity, clean up
					AD = it_lp->second.back().AD;
					if (rates[AD] >= 0) { // have demand
						it_lp->second.pop_back(); // remove it
					} else {
						//keep the -1 item, stop cleaning
						break;
					}
				}
			}
			while(1) { // find one that need to be allocated
				if (it_lp->second.empty()) { // this list is empty
					break;
				}
				AD = it_lp->second.back().AD;
				if (rates.find(AD) == rates.end() || rates[AD] == 0) { // already allocated this client
					it_lp->second.pop_back();
					continue;
				} else {
					break;
				}
			}
			if (it_lp->second.empty()) { // this list is empty
				continue;
			}
			/*find the smallest one*/
			if (it_best == latency_map.end() || it_best->second.back().latency > it_lp->second.back().latency) {
				it_best = it_lp;
			}
		}
		if (it_best == latency_map.end()) { // nothing left
			//syslog(LOG_ERR, "allocation done");
			break;
		} else {
			//syslog(LOG_ERR, "find best %s %d", it_best->second.back().AD.c_str(), it_best->second.back().latency);
		}
		/*allocate max capacity of the corresponding replica to the client with the smallest latency*/
		// we do this for all the client but only record the allocation for srcAD
		AD = it_best->second.back().AD;
		std::string replica = it_best->first;
		if (rates[AD] == -1) { // this is a zero request rate one, just allocate it to the closest replica
			if (AD == srcAD) {
				(*decision)[replica].percentage = 100; // any value
				//syslog(LOG_ERR, "allocate0 100%%: for %s", srcAD.c_str());
			}
			rates[AD] = 0; //mark it done
			it_best->second.pop_back();

		} else if (rates[AD] >= capacities[replica]) { // allocate the remaining capacity of that replica
			rates[AD] -= capacities[replica];
			it_best->second.pop_back();
			if (AD == srcAD) {
				(*decision)[replica].percentage = capacities[replica];
				//syslog(LOG_ERR, "allocate1 %d: for %s", capacities[replica], srcAD.c_str());
			}
			capacities[replica] = 0;
			while (!it_best->second.empty()) { // no more capacity, clean up
				AD = it_best->second.back().AD;
				if (rates[AD] >= 0) { // have demand
					it_best->second.pop_back(); // remove it
				} else {
					//keep the -1 item, stop cleaning
					break;
				}
			}
			//it_best->second.clear();
		} else { // capacity > rates[AD]
			capacities[replica] -= rates[AD];
			if (AD == srcAD) {
				(*decision)[replica].percentage = rates[AD];
				//syslog(LOG_ERR, "allocate2 %d: for %s", rates[AD], srcAD.c_str());

			}
			rates[AD] = 0;
			it_best->second.pop_back();
		}
	}
	//syslog(LOG_DEBUG, "Finish R_LB");

	//normalize the percentage
	int sum = 0;
	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // compute the sum
		sum += it_ad->second.percentage;
		//syslog(LOG_DEBUG, "record percentage %s %d", it_ad->first.c_str(), it_ad->second.percentage);
	}
	if (sum == 0) {
		syslog(LOG_ERR, "R_LB: sum = 0!");
		return -1;
	}
	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // update percentage
		it_ad->second.percentage = 100.0 * it_ad->second.percentage / sum + 0.5; // +0.5 to round
	}

	return 0;
}

int Controller::processSidDecisionAnswer(const Xroute::DecisionAnswerMsg& msg)
{
	// When got the answer, set local weight
	// The answer is per SID, when to update routing table?

	//syslog(LOG_DEBUG, "Processing SID decision");
	if (!ENABLE_SID_CTL) {
		// if SID control plane is disabled
		// we should not get such a ctl message
		return 0;
	}

	Xroute::XID xad  = msg.ad();
	Xroute::XID xhid = msg.hid();
	Xroute::XID xsid = msg.sid();

	string srcAD  = Node(xad.type(),  xad.id().c_str(),  0).to_string();
	string srcHID = Node(xhid.type(), xhid.id().c_str(), 0).to_string();
	string SID    = Node(xsid.type(), xsid.id().c_str(), 0).to_string();

	//syslog(LOG_INFO, "Get %s, %d answer msg from %s", SID.c_str(), records, srcAD.c_str());

	// process the entries: AD-latency pairs
	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
	it_sid = _SIDADsTable.find(SID);
	if (it_sid == _SIDADsTable.end()) {
		syslog(LOG_ERR, "No record for %s", SID.c_str());
		return -1;
	}

	std::map<std::string, ServiceState>::iterator it_ad;
	// check the to-be-deleted entries first
	for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad) {
		if (it_ad->second.priority < 0) {
			it_ad->second.percentage = -1;
		}
	}

	for (int i = 0; i < msg.sids_size(); i++) {
		Xroute::QueryAnswer q = msg.sids(i);

		xad = q.ad();
		string AD = Node(xad.type(),  xad.id().c_str(),  0).to_string();

		uint32_t percentage = q.percentage();

		//syslog(LOG_INFO, "Get %s, %d %%", AD.c_str(), percentage);
		std::map<std::string, ServiceState>::iterator it_ad = it_sid->second.find(AD);

		it_ad->second.valid = true; // valid is not implemented yet
		if (it_ad == it_sid->second.end()) {
			syslog(LOG_ERR, "No record for %s@%s", SID.c_str(), AD.c_str());
		} else {
			it_ad->second.percentage = percentage;
			if (percentage > 100) {
				syslog(LOG_ERR, "Invalid weight: %d", percentage);
			}
			if (it_ad->second.priority < 0) {
				syslog(LOG_ALERT, "To-be-deleted record received, %s@%s, it's OK", SID.c_str(), AD.c_str());
			}
		}
	}
/*
	for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad) {
		syslog(LOG_INFO, "DEBUG: %s weight: %d",it_ad->first.c_str(), it_ad->second.percentage );
	}
*/
	// for debug
	// dumpSidAdsTable();

	// update every router
	// temporally put it here.
	//sendSidRoutingDecision();

	return 0;
}


int Controller::processSidDecision(void) // to be deleted
{
	// make decision based on principles like highest priority first, load balancing, nearest...
	// Using function: (capacity^factor/link^factor)*priority for weight
	int rc = 1;

	//local balance decision: decide which percentage of traffic each AD should has
	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
	for (it_sid = _SIDADsTable.begin(); it_sid != _SIDADsTable.end(); ++it_sid)
	{
		// calculate the percentage for each AD for this SID
		double total_weight = 0;
		int minimal_latency = 9998;

		std::map<std::string, ServiceState>::iterator it_ad;
		std::string best_ad;
		// find the total weight
		for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
		{
			if (it_ad->second.priority < 0) { // this is the poisoned one, skip
				continue;
			}
			int latency = 9999; // default, 9999ms
			if (_ADPathStates.find(it_ad->first) != _ADPathStates.end()) {
				latency = _ADPathStates[it_ad->first].delay;
			}
			if (it_ad->second.capacity_factor == 0) { // only latency matters find the smallest
				minimal_latency = minimal_latency > latency?latency:minimal_latency;
				best_ad = minimal_latency == latency?it_ad->first:best_ad;
			}
			it_ad->second.weight = pow(it_ad->second.capacity, it_ad->second.capacity_factor)/
								   pow(latency, it_ad->second.link_factor)*it_ad->second.priority;
			total_weight += it_ad->second.weight;
			//syslog(LOG_INFO, "%s @%s :cap=%d, f=%d, late=%d, f=%d, prio=%d, weight is %f",it_sid->first.c_str(), it_ad->first.c_str(), it_ad->second.capacity, it_ad->second.capacity_factor, latency, it_ad->second.link_factor, it_ad->second.priority, it_ad->second.weight );
		}
			//syslog(LOG_INFO, "total_weight is %f", total_weight );
		// make local decision. map weights to 0..100
		for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
		{
			if (it_ad->second.priority < 0) { // this is the poisoned one
				//syslog(LOG_DEBUG, "Prepare to delete %s@%s\n", it_sid->first.c_str(), it_ad->first.c_str());
				it_ad->second.valid = true;
				it_ad->second.percentage = -1; //mark it to be deleted

			} else {
				it_ad->second.valid = true;
				if (it_ad->second.capacity_factor == 0) {// only latency matters
					it_ad->second.percentage = best_ad == it_ad->first?100:0; // go to the closest

				} else { // normal
					it_ad->second.percentage = (int) (100.0*it_ad->second.weight/total_weight+0.5);//round
				}
				if (it_ad->second.percentage > 100) {
					syslog(LOG_INFO, "Error setting weight%s @%s :cap=%d, f=%d, prio=%d, weight=%f, total weight=%f",it_sid->first.c_str(), it_ad->first.c_str(), it_ad->second.capacity, it_ad->second.capacity_factor, it_ad->second.priority, it_ad->second.weight, total_weight);
				}
			}
			//syslog(LOG_INFO, "percentage for %s@%s is %d",it_sid->first.c_str(),it_ad->first.c_str(), it_ad->second.percentage );
		}
	}
	// for debug
	// dumpSidAdsTable();

	// update every router
	//sendSidRoutingDecision();
	return rc;
}

int Controller::sendSidRoutingDecision(void)
{
	syslog(LOG_INFO, "Controller::Send-SID Decision");

	// TODO: when to call this function timeout? new incoming sid discovery?
	// TODO: if not reusing AD routing table, this function should calculate routing table
	// for each router
	// Now we just send the identical decision to every router. The routers will reuse their
	// own routing table to interpret the decision
	//syslog(LOG_DEBUG, "Processing SID decision routes");
	int rc = 1;

	// remap the SIDADsTable
	std::map<std::string, std::map<std::string, ServiceState> > ADSIDsTable;

	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
	for (it_sid = _SIDADsTable.begin(); it_sid != _SIDADsTable.end(); ++it_sid)
	{
		std::map<std::string, ServiceState>::iterator it_ad;
		for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
		{
			if (it_ad->second.valid) // only send choices we want to use
			{
				if (ADSIDsTable.count(it_ad->first) > 0) // has key AD
				{
					ADSIDsTable[it_ad->first][it_sid->first] = it_ad->second;

				} else {
				// create a new map for new AD
					std::map<std::string, ServiceState> new_map;
					new_map[it_sid->first] = it_ad->second;
					ADSIDsTable[it_ad->first] = new_map;
				}
			}
		}
	}

	// for each router:
	std::map<std::string, NodeStateEntry>::iterator it_router;
	for (it_router = _networkTable.begin(); it_router != _networkTable.end(); ++it_router)
	{
		if ((it_router->second.ad != _myAD) || (it_router->second.hid == ""))
		{
			// Don't calculate routes for external ADs
			continue;
		} else if (it_router->second.hid.find(string("SID")) != string::npos) {
			// Don't calculate routes for SIDs
			continue;
		}
		rc = sendSidRoutingTable(it_router->second.hid, ADSIDsTable);
	}
	return rc;
}

int Controller::sendSidRoutingTable(std::string destHID, std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable)
{
	syslog(LOG_INFO, "Controller::Send-Table");

	// send routing table to router:destHID. ADSIDsTable is a remapping of SIDADsTable for easier use
	if (destHID == _myHID)
	{
		// If destHID is self, process immediately
		//syslog(LOG_INFO, "set local sid routes");
		return processSidRoutingTable(ADSIDsTable);

	} else {
		// If destHID is not SID, send to relevant router
		//
		Xroute::XrouteMsg msg;



		msg.set_type()
		ControlMessage msg(CTL_SID_ROUTING_TABLE, _myAD, _myHID);

		// for checking and resend
		msg.append(_myAD);
		msg.append(destHID);
		msg.append(_sid_ctl_seq);

		//Format: #2 AD1:[ #2 SID1(percentage 30),SID2(percentage 100)], AD2[ #1 SID1(percentage 70)]

		msg.append(ADSIDsTable.size());
		std::map<std::string, std::map<std::string, ServiceState> >::iterator it_ad;
		for (it_ad = ADSIDsTable.begin(); it_ad != ADSIDsTable.end(); ++it_ad)
		{
			msg.append(it_ad->second.size());
			msg.append(it_ad->first);
			std::map<std::string, ServiceState>::iterator it_sid;
			for (it_sid = it_ad->second.begin(); it_sid != it_ad->second.end(); ++it_sid)
			{
				msg.append(it_sid->first);
				// TODO: put information from service_state inside
				msg.append(it_sid->second.percentage);
			}
		}

		return msg.send(_rsock, &_ddag);
	}
}

int Controller::processSidRoutingTable(std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable)
{
	int rc = 1;

	std::map<std::string, XIARouteEntry> xrt;
	_xr.getRoutes("AD", xrt);

	//change vector to map AD:RouteEntry for faster lookup
	std::map<std::string, XIARouteEntry> ADlookup;
	map<std::string, XIARouteEntry>::iterator ir;
	for (ir = xrt.begin(); ir != xrt.end(); ++ir) {
		ADlookup[ir->first] = ir->second;
	}

	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_ad;
	for (it_ad = ADSIDsTable.begin(); it_ad != ADSIDsTable.end(); ++it_ad)
	{
		XIARouteEntry entry = ADlookup[it_ad->first];
		//syslog(LOG_INFO, "AD entry: %s, %d, %s, %lu", entry.xid.c_str(), entry.port, entry.nextHop.c_str(), entry.flags);
		std::map<std::string, ServiceState>::iterator it_sid;
		for (it_sid = it_ad->second.begin(); it_sid != it_ad->second.end(); ++it_sid)
		{
			// TODO: how to use flags to load balance? or add new fields into routing table for this propose
			// syslog(LOG_INFO, "add route %s, %hu, %s, %lu", it_sid->first.c_str(), entry.port, entry.nextHop.c_str(), entry.flags);
			//if (it_sid->second.percentage <= 0)
			//    syslog(LOG_DEBUG, "Removing routing entry: %s@%s", it_sid->first.c_str(), entry.xid.c_str());

			if (entry.xid == _myAD)
			{
				_xr.seletiveSetRoute(it_sid->first, -2, entry.nextHop, entry.flags, it_sid->second.percentage, it_ad->first); //local AD, FIXME: why is port unsigned short? port could be negative numbers!
			} else {
				_xr.seletiveSetRoute(it_sid->first, entry.port, entry.nextHop, entry.flags, it_sid->second.percentage, it_ad->first);
			}

		}
	}

	return rc;
}

int Controller::updateSidAdsTable(std::string AD, std::string SID, ServiceState service_state)
{
	int rc = 1;
	//TODO: only record ones with newer version
	//TODO: only update publicly useful attributes
	//TODO: an entry is not there maybe because it is deleted,we should avoid that an older discovery message adds it back again
	if ( _SIDADsTable.count(SID) > 0 )
	{
		if (service_state.seq == 0) {
			syslog(LOG_DEBUG, "recording bad SID seq, abort");
			return -1;
		}
		if (_SIDADsTable[SID].count(AD) > 0 )
		{
			//update if version is newer
			if (_SIDADsTable[SID][AD].seq < service_state.seq || _SIDADsTable[SID][AD].seq - service_state.seq > SEQNUM_WINDOW)
			{
				//syslog(LOG_DEBUG, "Got %d > %d new discovery msg %s@%s, priority%d",service_state.seq, _SIDADsTable[SID][AD].seq, SID.c_str(), AD.c_str(), service_state.priority);
				if (_SIDADsTable[SID][AD].capacity != service_state.capacity) {
					_send_sid_decision = true; // update decision
				}
				_SIDADsTable[SID][AD].capacity = service_state.capacity;
				_SIDADsTable[SID][AD].capacity_factor = service_state.capacity_factor;
				_SIDADsTable[SID][AD].link_factor = service_state.link_factor;
				_SIDADsTable[SID][AD].priority = service_state.priority;
				_SIDADsTable[SID][AD].seq = service_state.seq;

			//TODO: update other parameters
			} else {
				//syslog(LOG_DEBUG, "Got %d < %d old discovery msg %s@%s, priority%d",service_state.seq, _SIDADsTable[SID][AD].seq, SID.c_str(), AD.c_str(), service_state.priority);
			}

		} else {
			// insert new entry
			_SIDADsTable[SID][AD] = service_state;
		}
	} else {
		// no sid record
		std::map<std::string, ServiceState> new_map;
		new_map[AD] = service_state;
		_SIDADsTable[SID] = new_map;
	}
	// TODO: make return value meaningful or make this function return void
	//syslog(LOG_INFO, "update SID:%s, capacity %d, f1 %d, f2 %d, priority %d, from %s", SID.c_str(), service_state.capacity, service_state.capacity_factor, service_state.link_factor, service_state.priority, AD.c_str());
	return rc;
}
#endif

void* Controller::updatePathThread(void* updating)
{
	UNUSED(updating);
#if 0
	// do the xping in a thread
	// updating is a bool* from the creator of this thread that indicates whether a new update thread is necessary
	// We do not want multiple update threads waiting if the updating is slower than the period the timer calls.
	// Hence, lock is not the approach we want
	// FIXME: using "xping 'RE ADx' " is wrong, we should get the latency to
	// the involved hosts in that AD, not the closest host from that AD.
	// FIXME: pinging the ADs one by one takes time, should make it parallel (multi-threaded)

	*((bool*) updating) = true; // do not reenter
	std::map<std::string, NodeStateEntry>::iterator it_ad;

	for (it_ad = _ADNetworkTable_temp.begin(); it_ad != _ADNetworkTable_temp.end(); ++it_ad)
	{
		if (it_ad->first == _myAD) {
			continue;
		}
		if (it_ad->first.length() < 3) { // abnormal entry, skip to avoid weird stuff
			continue;
		}

		//yslog(LOG_DEBUG, "send ping to %s", it_ad->first.c_str());
		FILE *in;
		char buff[BUF_SIZE];
		char cmd[BUF_SIZE] = "";
		char root[BUF_SIZE];
		int latency = -1;

		// send 5 pings (-c), interval 0.1s (-i), each timeout 1s (-t)
		// XSOCKCONF_SECTION makes sure xping is performed on the right host
		// Here we use -f 1 to find the minimal latency instead of average as ping delay is no stable in our current
		// implementation. Even small traffic like control messages can change the delay dramatically.
		sprintf(cmd,
			"XSOCKCONF_SECTION=%s %s/bin/xping -t 10 -q -c 5 'RE %s' | tail -1 | awk '{print $5}' | cut -d '/' -f 1",
			_hostname,
			XrootDir(root, BUF_SIZE),
			it_ad->first.c_str());
		if(!(in = popen(cmd, "r"))) {
			syslog(LOG_DEBUG, "Fail to execute %s", cmd);
			continue;
		}
		while(fgets(buff, sizeof(buff), in) != NULL) {
			if (strlen(buff) > 0) { // avoid bad things
				//syslog(LOG_DEBUG, "Ping %s result is %s",it_ad->first.c_str(), buff);
				latency = atoi(buff); // FIXME: atoi cannot detect error
			}
		}
		if (pclose(in) > 0) {
			syslog(LOG_DEBUG, "ping to %s timeout", it_ad->first.c_str());
		}
		if (latency >= 0) {
			//syslog(LOG_DEBUG, "latency to %s is %d", it_ad->first.c_str(), latency);
			ADPathState ADpath_state;
			ADpath_state.delay = latency>0?latency:9999; // maybe a timeout/atoi failure
			int delta = _ADPathStates[it_ad->first].delay - ADpath_state.delay;
			if ( delta > 5 || delta < -5 ) {
				_send_sid_decision = true; // re-query the decision because latency is changed
				//TODO: only query that SID.
			}
			//This is probably thread-safe TODO: lock
			_ADPathStates[it_ad->first] = ADpath_state;
		}
	}
	*((bool*) updating) = false; // enter
#endif
	pthread_exit(NULL);
}

int Controller::updateADPathStates(void)
{   // udpate the states of paths (latency) to each AD
	// FIXME: routers could have different latencies to the same AD, this
	// process should be done in each router
	//syslog(LOG_DEBUG, "updateADPathStates called , try to send to %lu ADs", _ADNetworkTable.size());
	int rc = 1;
	//Don't make the update when the previous one is ongoing
	static bool updating = false;

	// Create a new thread so that the daemon could receive control messages while waiting for xping
	pthread_t newthread;
	if (!updating) {
		// copy to local, avoid data hazard
		_ADNetworkTable_temp = _ADNetworkTable;
		if (pthread_create(&newthread , NULL, updatePathThread, &updating)!= 0) {
			perror("pthread_create");
		} else {
			pthread_detach(newthread); // leave the thread along
		}
	}


	// latency to itself is always 1ms (minimal value)
	// FIXME: above is not true!
	ADPathState ADpath_state;
	ADpath_state.delay = 1;
	_ADPathStates[_myAD] = ADpath_state;
	return rc;
}

int Controller::processHello(const Xroute::HelloMsg &msg, uint32_t iface)
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
	neighbor.AD        = neighborAD;
	neighbor.HID       = neighborHID;
	neighbor.port      = iface;
	neighbor.flags     = flags;
	neighbor.cost      = 1; // for now, same cost
	neighbor.timestamp = time(NULL);

	// Index by HID if neighbor in same domain or by AD otherwise
	bool internal = (neighbor.AD == _myAD);
	_neighborTable[internal ? neighbor.HID : neighbor.AD] = neighbor;
	_num_neighbors = _neighborTable.size();

	// Update network table
	std::string myHID = _myHID;

	NodeStateEntry entry;
	entry.hid = myHID;
	entry.num_neighbors = _num_neighbors;

	// Add neighbors to network table entry
	std::map<std::string, NeighborEntry>::iterator it;
	for (it = _neighborTable.begin(); it != _neighborTable.end(); it++)
		entry.neighbor_list.push_back(it->second);

	_networkTable[myHID] = entry;
	syslog(LOG_INFO, "Process-Hello[%s]", neighbor.HID.c_str());

	return 1;
}

int Controller::processRoutingTable(std::map<std::string, RouteEntry> routingTable)
{
	int rc;
	map<string, RouteEntry>::iterator it;
	for (it = routingTable.begin(); it != routingTable.end(); it++)
	{
		// TODO check for all published SIDs
		// TODO do this for xrouted as well
		// Ignore SIDs that we publish
		if (it->second.dest == controller_sid) {
			continue;
		}
		if ((rc = _xr.setRoute(it->second.dest, it->second.port, it->second.nextHop, it->second.flags)) != 0)
			syslog(LOG_ERR, "error setting route %d", rc);

		_timeStamp[it->second.dest] = time(NULL);
	}

	return 1;
}

/* Procedure:
   0. scan this LSA (mark AD with a DualRouter if there)
   1. filter out the already seen LSA (via LSA-seq for this dest)
   2. update the network table
   3. rebroadcast this LSA
*/
int Controller::processLSA(const Xroute::LSAMsg& msg)
{
	Xroute::XID a = msg.node().ad();
	Xroute::XID h = msg.node().hid();

	string srcAD  = Node(a.type(), a.id().c_str(), 0).to_string();
	string srcHID = Node(h.type(), h.id().c_str(), 0).to_string();

	if (msg.has_flags() && msg.flags() & F_IP_GATEWAY) {
		_dual_router_AD = srcAD;
	}

	if (srcHID == _myHID) {
		return 1;
	}

	uint32_t numNeighbors = msg.peers_size();

	// 2. Update the network table
	NodeStateEntry entry;
	entry.ad  = srcAD;
	entry.hid = srcHID;
	entry.num_neighbors = numNeighbors;

	for (uint32_t i = 0; i < numNeighbors; i++) {
		NeighborEntry neighbor;
		Xroute::NeighborEntry n = msg.peers(i);

		a = n.ad();
		h = n.hid();
		neighbor.AD   = Node(a.type(), a.id().c_str(), 0).to_string();
		neighbor.HID  = Node(h.type(), h.id().c_str(), 0).to_string();
		neighbor.port = n.port();
		neighbor.cost = n.cost();

		if (neighbor.AD != _myAD) { // update neighbors
			neighbor.timestamp = time(NULL);
			_ADNeighborTable[neighbor.AD] = neighbor;
			_ADNeighborTable[neighbor.AD].HID = neighbor.AD; // make the algorithm work
		}

		entry.neighbor_list.push_back(neighbor);
	}
	extractNeighborADs();
	_networkTable[srcHID] = entry;
	_calc_dijstra_ticks++;

	if (_calc_dijstra_ticks >= CALC_DIJKSTRA_INTERVAL || _calc_dijstra_ticks  < 0)
	{
		// syslog(LOG_DEBUG, "Calcuating shortest paths\n");

		// Calculate next hop for ADs
		std::map<std::string, RouteEntry> ADRoutingTable;
		populateRoutingTable(_myAD, _ADNetworkTable, ADRoutingTable);

		// For debugging.
		// printADNetworkTable();
		// printRoutingTable(_myAD, ADRoutingTable);

		// Calculate next hop for routers
		std::map<std::string, NodeStateEntry>::iterator it1;
		// Iterate through ADs
		for (it1 = _networkTable.begin(); it1 != _networkTable.end(); ++it1)
		{
			if ((it1->second.ad != _myAD) || (it1->second.hid == "")) {
				// Don't calculate routes for external ADs
				continue;
			} else if (it1->second.hid.find(string("SID")) != string::npos) {
				// Don't calculate routes for SIDs
				continue;
			}
			std::map<std::string, RouteEntry> routingTable;

			// Calculate routing table for HIDs instead it1 AD
			populateRoutingTable(it1->second.hid, _networkTable, routingTable);
			//extractNeighborADs(routingTable);
			populateNeighboringADBorderRouterEntries(it1->second.hid, routingTable);
			populateADEntries(routingTable, ADRoutingTable);
			//printRoutingTable(it1->second.hid, routingTable);

			sendRoutingTable(it1->second.hid, routingTable);
		}

		// HACK HACK HACK!!!!
		// for some reason some controllers will delete their routing table entries in
		// populateRoutingTable when they shouldn't. This forces the entries back into the table
		// it's the same logic from inside the loop above
		std::map<std::string, RouteEntry> routingTable;
		populateRoutingTable(_myHID, _networkTable, routingTable);
		populateNeighboringADBorderRouterEntries(_myHID, routingTable);
		populateADEntries(routingTable, ADRoutingTable);
		sendRoutingTable(_myHID, routingTable);
		// END HACK

		_calc_dijstra_ticks = _calc_dijstra_ticks >0?0:_calc_dijstra_ticks;
	}

	return 1;
}

// Extract neighboring AD from the routing table
int Controller::extractNeighborADs(void)
{
	// Update network table
	std::string myAD = _myAD;

	NodeStateEntry entry;
	entry.ad = myAD;
	entry.hid = myAD;
	entry.num_neighbors = _ADNeighborTable.size();
	entry.timestamp = time(NULL);

	// Add neighbors to network table entry
	std::map<std::string, NeighborEntry>::iterator it1;
	for (it1 = _ADNeighborTable.begin(); it1 != _ADNeighborTable.end(); ++it1)
		entry.neighbor_list.push_back(it1->second);

	_ADNetworkTable[myAD] = entry;
	return 1;
}

void Controller::populateNeighboringADBorderRouterEntries(string currHID, std::map<std::string, RouteEntry> &routingTable)
{
	vector<NeighborEntry> currNeighborTable = _networkTable[currHID].neighbor_list;

	vector<NeighborEntry>::iterator it;
	for (it = currNeighborTable.begin(); it != currNeighborTable.end(); ++it) {
		if (it->AD != _myAD) {
			// Add HID of border routers of neighboring ADs into routing table
			string neighborHID = it->HID;
			RouteEntry &entry = routingTable[neighborHID];
			entry.dest = neighborHID;
			entry.nextHop = neighborHID;
			entry.port = it->port;
			//entry.flags = 0;
		}
	}
}

void Controller::populateADEntries(std::map<std::string, RouteEntry> &routingTable, std::map<std::string, RouteEntry> ADRoutingTable)
{
	std::map<std::string, RouteEntry>::iterator it1;  // Iter for route table

	for (it1 = ADRoutingTable.begin(); it1 != ADRoutingTable.end(); it1++) {
		string destAD = it1->second.dest;
		string nextHopAD = it1->second.nextHop;

		RouteEntry &entry = routingTable[destAD];
		entry.dest = destAD;
		entry.nextHop = routingTable[nextHopAD].nextHop;
		entry.port = routingTable[nextHopAD].port;
		entry.flags = routingTable[nextHopAD].flags;
	}
}

// Run Dijkstra shortest path algorithm, and populate the next hops.
// This code is hacky to support AD and HID. This can be rewritten better.
void Controller::populateRoutingTable(std::string srcHID, std::map<std::string, NodeStateEntry> &networkTable, std::map<std::string, RouteEntry> &routingTable)
{
	std::map<std::string, NodeStateEntry>::iterator it1;  // Iter for network table
	std::vector<NeighborEntry>::iterator it2;             // Iter for neighbor list

	map<std::string, NodeStateEntry> unvisited;  // Set of unvisited nodes

	routingTable.clear();

	// Filter out anomalies
	//@ (When do these appear? Should they not be introduced in the first place? How about SIDs?)
	it1 = networkTable.begin();
	while (it1 != networkTable.end()) {
		if (it1->second.num_neighbors == 0 || it1->second.ad.empty() || it1->second.hid.empty()) {
			networkTable.erase(it1++);
		} else {
			//syslog(LOG_DEBUG, "entry %s: neighbors: %d", it1->first.c_str(), it1->second.neighbor_list.size());
			++it1;
		}
	}

	unvisited = networkTable;

	// Initialize Dijkstra variables for all nodes
	for (it1=networkTable.begin(); it1 != networkTable.end(); it1++) {
		it1->second.checked = false;
		it1->second.cost = 10000000;
	}

	string currXID;

	// Visit root node (srcHID)
	unvisited.erase(srcHID);
	networkTable[srcHID].checked = true;
	networkTable[srcHID].cost = 0;

	// Process neighboring nodes of root node
	for (it2 = networkTable[srcHID].neighbor_list.begin(); it2 < networkTable[srcHID].neighbor_list.end(); it2++) {
		currXID = (it2->AD == _myAD) ? it2->HID : it2->AD;

		if (networkTable.find(currXID) != networkTable.end()) {
			networkTable[currXID].cost = it2->cost;
			networkTable[currXID].prevNode = srcHID;

		} else {
			// We have an endhost
			NeighborEntry neighbor;
			neighbor.AD = _myAD;
			neighbor.HID = srcHID;
			neighbor.port = 0; // Endhost only has one port
			neighbor.cost = 1;

			NodeStateEntry entry;
			entry.ad = it2->AD;
			entry.hid = it2->HID;
			entry.num_neighbors = 1;
			entry.neighbor_list.push_back(neighbor);
			entry.cost = neighbor.cost;
			entry.prevNode = neighbor.HID;

			networkTable[currXID] = entry;
		}
	}

	// Loop until all nodes have been visited
	while (!unvisited.empty()) {
		int minCost = 10000000;
		string selectedHID;
		// Select unvisited node with min cost
		for (it1=unvisited.begin(); it1 != unvisited.end(); ++it1) {
			//syslog(LOG_DEBUG, "it1 %s, %s", it1->second.ad.c_str(), it1->second.hid.c_str());
			currXID = (it1->second.ad == _myAD) ? it1->second.hid : it1->second.ad;
			//syslog(LOG_DEBUG, "CurrXID is %s, cost %d", currXID.c_str(), networkTable[currXID].cost);
			if (networkTable[currXID].cost < minCost) {
				minCost = networkTable[currXID].cost;
				selectedHID = currXID;
			}
		}
		if(selectedHID.empty()) {
			// Rest of the nodes cannot be reached from the visited set
			//syslog(LOG_DEBUG, "%s has an empty routingTable", srcHID.c_str());
			break;
		}

		// Remove selected node from unvisited set
		unvisited.erase(selectedHID);
		networkTable[selectedHID].checked = true;

		// Process all unvisited neighbors of selected node
		for (it2 = networkTable[selectedHID].neighbor_list.begin(); it2 != networkTable[selectedHID].neighbor_list.end(); it2++) {
			currXID = (it2->AD == _myAD) ? it2->HID : it2->AD;
			if (networkTable[currXID].checked != true) {
				if (networkTable[currXID].cost > networkTable[selectedHID].cost + 1) {
					//@ Why add 1 to cost instead of using link cost from neighbor_list?
					networkTable[currXID].cost = networkTable[selectedHID].cost + 1;
					networkTable[currXID].prevNode = selectedHID;
				}
			}
		}
	}

	// For each destination ID, find the next hop ID and port by going backwards along the Dijkstra graph
	string tempHID1;			// ID of destination in srcHID's routing table
	string tempHID2;			// ID of node currently being processed
	string tempNextHopHID2;		// HID of next hop to reach destID from srcHID
	int hop_count;

	for (it1 = networkTable.begin(); it1 != networkTable.end(); it1++) {
		if (unvisited.find(it1->first) !=  unvisited.end()) { // the unreachable set
			continue;
		}
		tempHID1 = (it1->second.ad == _myAD) ? it1->second.hid : it1->second.ad;
		if (tempHID1.find(string("SID")) != string::npos) {
			// Skip SIDs on first pass
			continue;
		}
		if (srcHID.compare(tempHID1) != 0) {
			tempHID2 = tempHID1;
			tempNextHopHID2 = it1->second.hid;
			hop_count = 0;
			while (networkTable[tempHID2].prevNode.compare(srcHID)!=0 && hop_count < MAX_HOP_COUNT) {
				tempHID2 = networkTable[tempHID2].prevNode;
				tempNextHopHID2 = networkTable[tempHID2].hid;
				hop_count++;
			}
			if (hop_count < MAX_HOP_COUNT) {
				routingTable[tempHID1].dest = tempHID1;
				routingTable[tempHID1].nextHop = tempNextHopHID2;
				routingTable[tempHID1].flags = 0;

				// Find port of next hop
				for (it2 = networkTable[srcHID].neighbor_list.begin(); it2 < networkTable[srcHID].neighbor_list.end(); it2++) {
					if (((it2->AD == _myAD) ? it2->HID : it2->AD) == tempHID2) {
						routingTable[tempHID1].port = it2->port;
					}
				}
			}
		}
	}

	for (it1 = networkTable.begin(); it1 != networkTable.end(); it1++) {
		tempHID1 = (it1->second.ad == _myAD) ? it1->second.hid : it1->second.ad;
		if (unvisited.find(it1->first) !=  unvisited.end()) { // the unreachable set
			continue;
		}
		if (tempHID1.find(string("SID")) == string::npos) {
			// Process SIDs on second pass
			continue;
		}
		if (srcHID.compare(tempHID1) != 0) {
			tempHID2 = tempHID1;
			tempNextHopHID2 = it1->second.hid;
			hop_count = 0;
			while (networkTable[tempHID2].prevNode.compare(srcHID)!=0 && hop_count < MAX_HOP_COUNT) {
				tempHID2 = networkTable[tempHID2].prevNode;
				tempNextHopHID2 = networkTable[tempHID2].hid;
				hop_count++;
			}
			if (hop_count < MAX_HOP_COUNT) {
				routingTable[tempHID1].dest = tempHID1;

				// Find port of next hop
				for (it2 = networkTable[srcHID].neighbor_list.begin(); it2 < networkTable[srcHID].neighbor_list.end(); it2++) {
					if (((it2->AD == _myAD) ? it2->HID : it2->AD) == tempHID2) {
						routingTable[tempHID1].port = it2->port;
					}
				}

				// Dest is SID, so we search existing ports for entry with same port and HID as next hop
				bool entryFound = false;
				map<string, RouteEntry>::iterator it3;
				for (it3 = routingTable.begin(); it3 != routingTable.end(); it3++) {
					if (it3->second.port == routingTable[tempHID1].port && it3->second.nextHop.find(string("HID")) != string::npos) {
						routingTable[tempHID1].nextHop = it3->second.nextHop;
						routingTable[tempHID1].flags = 0;
						entryFound = true;
						break;
					}
				}
				if (!entryFound) {
					// Delete SID entry from routingTable
					routingTable.erase(tempHID1);
				}
			}
		}
	}
	//printRoutingTable(srcHID, routingTable);
}

void Controller::printRoutingTable(std::string srcHID, std::map<std::string, RouteEntry> &routingTable)
{
	syslog(LOG_INFO, "Routing table for %s", srcHID.c_str());
	map<std::string, RouteEntry>::iterator it1;
	for ( it1=routingTable.begin() ; it1 != routingTable.end(); it1++ ) {
		syslog(LOG_INFO, "Dest=%s, NextHop=%s, Port=%d, Flags=%u", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags) );
	}
}

void Controller::printADNetworkTable()
{
	syslog(LOG_INFO, "Network table for %s:", _myAD.c_str());
	std::map<std::string, NodeStateEntry>::iterator it;
	for (it = _ADNetworkTable.begin();
			it != _ADNetworkTable.end(); it++) {
		syslog(LOG_INFO, "%s", it->first.c_str());
		for (size_t i = 0; i < it->second.neighbor_list.size(); i++) {
			syslog(LOG_INFO, "neighbor[%d]: %s", (int) i,
					it->second.neighbor_list[i].AD.c_str());
		}
	}

}

void Controller::set_controller_conf(const char* myhostname)
{
	// read the controller file at etc/controller.ini
	// set the parameters
	char full_path[BUF_SIZE];
	char root[BUF_SIZE];
	char section_name[BUF_SIZE];
	bool mysection = false; // read default section or my section

	snprintf(full_path, BUF_SIZE, "%s/etc/controllers.ini", XrootDir(root, BUF_SIZE));
	int section_index = 0;
	while (ini_getsection(section_index, section_name, BUF_SIZE, full_path))
	{
		if (strcmp(section_name, myhostname) == 0) // section for me
		{
			mysection = true;
			break;
		}
		section_index++;
	}
	if (mysection) {
		strcpy(section_name, myhostname);
	} else {
		strcpy(section_name, "default");
	}
	// read the values
	expire_time = ini_getl(section_name, "expire_time", EXPIRE_TIME, full_path);
	HELLO_INTERVAL = ini_getf(section_name, "hello_interval", HELLO_INTERVAL_D, full_path);
	LSA_INTERVAL = ini_getf(section_name, "LSA_interval", LSA_INTERVAL_D, full_path);
	SID_DISCOVERY_INTERVAL = ini_getf(section_name, "SID_discovery_interval", SID_DISCOVERY_INTERVAL_D, full_path);
	SID_DECISION_INTERVAL = ini_getf(section_name, "SID_decision_interval", SID_DECISION_INTERVAL_D, full_path);
	AD_LSA_INTERVAL = ini_getf(section_name, "AD_LSA_interval", AD_LSA_INTERVAL_D, full_path);
	CALC_DIJKSTRA_INTERVAL = ini_getf(section_name, "calc_Dijkstra_interval", CALC_DIJKSTRA_INTERVAL_D, full_path);
	MAX_HOP_COUNT  = ini_getl(section_name, "max_hop_count", MAX_HOP_COUNT_D, full_path);
	MAX_SEQNUM = ini_getl(section_name, "max_sqenum", MAX_SEQNUM_D, full_path);
	SEQNUM_WINDOW = ini_getl(section_name, "sqenum_window", SEQNUM_WINDOW_D, full_path);
	UPDATE_CONFIG = ini_getl(section_name, "update_config", UPDATE_CONFIG_D, full_path);
	UPDATE_LATENCY = ini_getl(section_name, "update_latency", UPDATE_LATENCY_D, full_path);
	ENABLE_SID_CTL = ini_getl(section_name, "enable_SID_ctl", ENABLE_SID_CTL_D, full_path);

	return;
}


void Controller::set_sid_conf(const char* myhostname)
{
	UNUSED(myhostname);
#if 0
	// read the controller file at etc/controller$i_sid.ini
	// the file defines which AD has what SIDs
	// ini style file
	// the file could be loaded frequently to emulate dynamic service changes

	// time to have some update of the local services
	_sid_discovery_seq = (_sid_discovery_seq + 1) % MAX_SEQNUM;

	char full_path[BUF_SIZE];
	char root[BUF_SIZE];
	char section_name[BUF_SIZE];
	char controller_addr[BUF_SIZE];
	char sid[BUF_SIZE];
	std::string service_sid;

	int section_index = 0;
	snprintf(full_path, BUF_SIZE, "%s/etc/%s_sid.ini", XrootDir(root, BUF_SIZE), myhostname);
	std::map<std::string, ServiceState> old_local_list = _LocalSidList; // to check if some entries is being removed. NOTE: may need deep copy in the future or more efficient way to do this
	_LocalSidList.clear(); // clean and reload all

	while (ini_getsection(section_index, section_name, BUF_SIZE, full_path)) //enumerate every section, [$name]
	{
		//fprintf(stderr, "read %s\n", section_name);
		ini_gets(section_name, "sid", sid, sid, BUF_SIZE, full_path);
		service_sid = std::string(sid);

		/*first read the synthetic load as a client domain*/
		int s_load = ini_getl(section_name, "synthetic_load", 0, full_path);
		if (s_load == 0) { // synthetic load is not set, make sure it won't overwrite the existing value
			if (_SIDRateTable.find(service_sid) == _SIDRateTable.end()) {
				_SIDRateTable[service_sid] = 0;
			} else {
				// do nothing
			}
		} else {
			_SIDRateTable[service_sid] = s_load;
		}


		if (ini_getbool(section_name, "enabled", 0, full_path) == 0)
		{// skip this if not enabled, NOTE: enabled=True is the default one
		 // if an entry was enabled but is disabled now, we should 'poison' other ADs by broadcasting this invalidation
			if (old_local_list.count(service_sid) > 0) { // it was there
				old_local_list[service_sid].priority = -1; // invalid entry
				old_local_list[service_sid].seq = _sid_discovery_seq;
				_LocalSidList[service_sid] = old_local_list[service_sid]; // NOTE: may need deep copy someday
				//syslog(LOG_DEBUG, "Poisoning %s\n", sid);
			}
			//else just ignore it
		} else {
			ServiceState service_state;
			service_state.seq = _sid_discovery_seq;
			service_state.capacity = ini_getl(section_name, "capacity", 100, full_path);
			service_state.capacity_factor = ini_getl(section_name, "capacity_factor", 1, full_path);
			service_state.link_factor = ini_getl(section_name, "link_factor", 0, full_path);
			service_state.priority = ini_getl(section_name, "priority", 1, full_path);
			service_state.isLeader = ini_getbool(section_name, "isleader", 0, full_path);
			// get the address of the service controller
			ini_gets(section_name, "leaderaddr", controller_addr, controller_addr, BUF_SIZE, full_path);
			service_state.leaderAddr = std::string(controller_addr);
			if (service_state.isLeader) // I am the leader
			{
				if (_LocalServiceLeaders.find(service_sid) == _LocalServiceLeaders.end())
				{ // no service controller yet, create one
					ServiceLeader sc;
					_LocalServiceLeaders[service_sid] = sc; // just initialize it
				}
			}
			service_state.archType = ini_getl(section_name, "archType", 0, full_path);
			service_state.priority = ini_getl(section_name, "priority", 1, full_path);
			service_state.internal_delay = ini_getl(section_name, "internaldelay", 0, full_path);
			int decision_type = ini_getl(section_name, "decisiontype", 0, full_path);
			switch (decision_type)
			{
				case LATENCY_FIRST:
					service_state.decision = &Controller::Latency_first;
					break;
				case PURE_LOADBALANCE:
					service_state.decision = &Controller::Load_balance;
					break;
				case RATE_LOADBALANCE:
					service_state.decision = &Controller::Rate_load_balance;
					break;
				default:
					syslog(LOG_DEBUG, "unknown decision function %s\n", sid);
			}

			//fprintf(stderr, "read state%s, %d, %d, %s\n", sid, service_state.capacity, service_state.isLeader, service_state.leaderAddr.c_str());
			if (_LocalSidList[service_sid].capacity != service_state.capacity) {
				_send_sid_discovery = true; //update
			}
			_LocalSidList[service_sid] = service_state;
		}

		section_index++;
	}

	return;
#endif
}

int Controller::processMsg(std::string msg_str, uint32_t iface)
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
		printf("is this gonna work for bad data\n");
	}

	switch (msg.type()) {
		case Xroute::HELLO_MSG:
			// do we still need this in the controller?
			rc = processHello(msg.hello(), iface);
			break;

		case Xroute::LSA_MSG:
			rc = processLSA(msg.lsa());
			break;

		case Xroute::GLOBAL_LSA_MSG:
			rc = processInterdomainLSA(msg.global_lsa());
			break;


		case Xroute::SID_MANAGE_KA_MSG:
			rc = processServiceKeepAlive(msg.keep_alive());
			break;

		//////////////////////////////////////////////////////////////////////
		// ignore
		case Xroute::CONFIG_MSG:
		case Xroute::HOST_JOIN_MSG:
			syslog(LOG_INFO, "controller received a config message");
			break;

		case Xroute::SID_TABLE_UPDATE_MSG:
		case Xroute::TABLE_UPDATE_MSG:
		case Xroute::HOST_LEAVE_MSG:
			syslog(LOG_INFO, "controller received a router message");
			break;

		//////////////////////////////////////////////////////////////////////
		// not sure if these should be running again
		case Xroute::SID_DISCOVERY_MSG:
//			rc = processSidDiscovery(msg.sid_discovery());
			break;

		case Xroute::SID_DECISION_QUERY_MSG:
//			rc = processSidDecisionQuery(msg.sid_query());
			break;

		case Xroute::SID_DECISION_ANSWER_MSG:
//			rc = processSidDecisionAnswer(msg.sid_answer());
			break;

		//////////////////////////////////////////////////////////////////////
		// not sure these even exist
		case Xroute::SID_RE_DISCOVERY_MSG:
		case Xroute::AD_PATH_STATE_PING_MSG:
		case Xroute::AD_PATH_STATE_PONG_MSG:
			syslog(LOG_INFO, "controller received a limbo message");
			break;

		default:
			syslog(LOG_INFO, "controller received an unknown message type");
			break;
	}

	return rc;
}
