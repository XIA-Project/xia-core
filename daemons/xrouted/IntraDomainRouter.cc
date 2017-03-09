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
	int selectRetVal, n;
	socklen_t dlen;
	char recv_message[10240];
	sockaddr_x theirDAG;
	fd_set socks;
	struct timeval timeoutval;
	vector<string> routers;
	time_t last_purge, hello_last_purge;

	struct timeval now;
	timeoutval.tv_sec = 0;
	timeoutval.tv_usec = 50000; // every 0.05 sec, check if any received packets

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

		FD_ZERO(&socks);
		FD_SET(_sock, &socks);

		selectRetVal = Xselect(_sock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			// receiving a Hello or LSA packet
			memset(&recv_message[0], 0, sizeof(recv_message));
			dlen = sizeof(sockaddr_x);
			n = Xrecvfrom(_sock, recv_message, 10240, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
				perror("recvfrom");
			}

			std::string msg = recv_message;
			processMsg(msg);
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

int IntraDomainRouter::interfaceNumber(std::string xidType, std::string xid)
{
	int rc;
	map<std::string, XIARouteEntry> routes;
	map<std::string, XIARouteEntry>::iterator it;

	if ((rc = _xr.getRoutes(xidType, routes)) > 0) {
		it = routes.find(xid);

		if (it != routes.end()) {
			return it->second.port;
		}
	}
	return -1;
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

	// mark if this is a dual XIA-IPv4 router
	if( XisDualStackRouter(_sock) == 1 ) {
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

	gettimeofday(&now, NULL);
	timeradd(&now, &h_freq, &h_fire);
	timeradd(&now, &l_freq, &l_fire);

	return 0;
}

int IntraDomainRouter::sendHello()
{
	ControlMessage msg(CTL_HELLO, _myAD, _myHID);
	//syslog(LOG_INFO, "Send-Hello");

	return msg.send(_sock, &_ddag);
}

// send LinkStateAdvertisement message (flooding)
int IntraDomainRouter::sendLSA()
{
	ControlMessage msg(CTL_LSA, _myAD, _myHID);

	msg.append(_dual_router);
	msg.append(_lsa_seq);
	msg.append(_num_neighbors);

	std::map<std::string, NeighborEntry>::iterator it;
	for (it = _neighborTable.begin(); it != _neighborTable.end(); ++it) {
		msg.append(it->second.AD);
		msg.append(it->second.HID);
		msg.append(it->second.port);
		msg.append(it->second.cost);
	}

	_lsa_seq = (_lsa_seq + 1) % MAX_SEQNUM;

	//syslog(LOG_INFO, "Send-LSA[%d]", _lsa_seq);
	return msg.send(_sock, &_ddag);
}

int IntraDomainRouter::processMsg(std::string msg)
{
	int type, rc = 0;
	ControlMessage m(msg);

	m.read(type);
	switch (type) {
		case CTL_HOST_REGISTER:
			rc = processHostRegister(m);
			break;
		case CTL_HELLO:
			rc = processHello(m);
			break;
		case CTL_LSA:
			rc = processLSA(m);
			break;
		case CTL_ROUTING_TABLE:
			rc = processRoutingTable(m);
			break;
		case CTL_SID_ROUTING_TABLE:
			rc = processSidRoutingTable(m);
			break;
		default:
			perror("unknown routing message");
			break;
	}

	return rc;
}

int IntraDomainRouter::processSidRoutingTable(ControlMessage msg)
{
	int rc = 1;
	int ad_count = 0;
	int sid_count = 0;
	int weight = 0;
	int ctlSeq;
	string srcAD, srcHID, destAD, destHID;

	string AD;
	string SID;

	msg.read(srcAD);
	msg.read(srcHID);

	if (srcAD != _myAD)
		return 1;

	msg.read(destAD);
	msg.read(destHID);
	msg.read(ctlSeq);

	// Check if intended for me
	if (destAD != _myAD) {
		return 1;
	}

	if (destHID != _myHID) {
		// only broadcast one time for each
		int his_ctl_seq = _sid_ctl_seqs[destHID]; // NOTE: default value of int is 0
		if (ctlSeq <= his_ctl_seq && his_ctl_seq - ctlSeq < SEQNUM_WINDOW) {
			// seen it before
			return 1;
		} else {
			_sid_ctl_seqs[destHID] = ctlSeq;
			return msg.send(_sock, &_ddag);
		}
	}

	if (ctlSeq <= _sid_ctl_seq && _sid_ctl_seq - ctlSeq < SEQNUM_WINDOW) {
		return 1;
	}
	_sid_ctl_seq = ctlSeq;

	std::map<std::string, XIARouteEntry> xrt;
	_xr.getRoutes("AD", xrt);

	// change vector to map AD:RouteEntry for faster lookup
	std::map<std::string, XIARouteEntry> ADlookup;
	map<std::string, XIARouteEntry>::iterator ir;
	for (ir = xrt.begin(); ir != xrt.end(); ++ir) {
		ADlookup[ir->second.xid] = ir->second;
	}

	msg.read(ad_count);
	for ( int i = 0; i < ad_count; ++i) {
		msg.read(sid_count);
		msg.read(AD);
		XIARouteEntry entry = ADlookup[AD];
		for ( int j = 0; j < sid_count; ++j) {
			msg.read(SID);
			msg.read(weight);
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


int IntraDomainRouter::processHostRegister(ControlMessage msg)
{
	int rc;
	NeighborEntry neighbor;
	neighbor.AD = _myAD;
	msg.read(neighbor.HID);
	neighbor.port = interfaceNumber("HID", neighbor.HID);
	neighbor.cost = 1; // for now, same cost

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
	if ((rc = _xr.setRoute(neighbor.HID, neighbor.port, neighbor.HID, 0)) != 0) {
		syslog(LOG_ERR, "unable to set host route: %s (%d)", neighbor.HID.c_str(), rc);
	}

	_hello_timeStamp[neighbor.HID] = time(NULL);

	return 1;
}

int IntraDomainRouter::processHello(ControlMessage msg)
{
	string HID;
	string SID;

	// Update neighbor table
	NeighborEntry neighbor;
	msg.read(neighbor.AD);
	msg.read(HID);
	if (msg.read(SID) < 0) {
		neighbor.HID = HID;
	} else {
		neighbor.HID = SID;
	}
	neighbor.port = interfaceNumber("HID", HID);
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

int IntraDomainRouter::processLSA(ControlMessage msg)
{
	std::string srcAD;
	std::string srcHID;
	int32_t dualRouter;
	int32_t lastSeq;

	msg.read(srcAD);
	msg.read(srcHID);
	msg.read(dualRouter);
	msg.read(lastSeq);

	syslog(LOG_INFO, "Process-LSA: [%d] %s %s", lastSeq, srcAD.c_str(), srcHID.c_str());

	if (srcAD != _myAD)
		return 1;

	if (_lastSeqTable.find(srcHID) != _lastSeqTable.end()) {
		int32_t old = _lastSeqTable[srcHID];
		if (lastSeq <= old && (old - lastSeq) < SEQNUM_WINDOW) {
			// drop the old LSA update.
			syslog(LOG_INFO, "Drop-LSA: [%d] %s %s", lastSeq, srcAD.c_str(), srcHID.c_str());
			return 1;
		}
	}

	syslog(LOG_INFO, "Forward-LSA: [%d] %s %s", lastSeq, srcAD.c_str(), srcHID.c_str());
	_lastSeqTable[srcHID] = lastSeq;

	// 5. rebroadcast this LSA
	return msg.send(_sock, &_ddag);
}

// process a control message
int IntraDomainRouter::processRoutingTable(ControlMessage msg)
{
	// 0. Read this LSA
	string srcAD, srcHID, destAD, destHID, hid, nextHop;
	int ctlSeq, numEntries, port, flags, rc;

	msg.read(srcAD);
	msg.read(srcHID);

	// Check if this came from our controller
	if (srcAD != _myAD)
		return 1;

	msg.read(destAD);
	msg.read(destHID);
	msg.read(ctlSeq);

	// 1. Filter out the already seen LSA
	// If this LSA already seen, ignore this LSA; do nothing

	// Check if intended for me
	if ((destAD != _myAD) || (destHID != _myHID)) {
		// only broadcast one time for each
		int his_ctl_seq = _ctl_seqs[destHID]; // NOTE: default value of int is 0
		if (ctlSeq <= his_ctl_seq && his_ctl_seq - ctlSeq < 10000) {
			// seen it before
			return 1;
		} else {
			_ctl_seqs[destHID] = ctlSeq;
			return msg.send(_sock, &_ddag);
		}
	}

	if (ctlSeq <= _ctl_seq && _ctl_seq - ctlSeq < 10000) {
		return 1;
	}
	_ctl_seq = ctlSeq;

	msg.read(numEntries);

	int i;
	for (i = 0; i < numEntries; i++) {
		msg.read(hid);
		msg.read(nextHop);
		msg.read(port);
		msg.read(flags);

		if ((rc = _xr.setRoute(hid, port, nextHop, flags)) != 0)
			syslog(LOG_ERR, "error setting route %d: %s, nextHop: %s, port: %d, flags %d", rc, hid.c_str(), nextHop.c_str(), port, flags);

		_timeStamp[hid] = time(NULL);
	}

	return 1;
}
