#include "xcidrouted.h"

using namespace std;

static char *ident = NULL;
static char* hostname = NULL;

static XIARouter xr;
static RouteState routeState;

HelloMessage::HelloMessage(){};
HelloMessage::~HelloMessage(){};

string HelloMessage::serialize() const{
	string result;

	result += this->AD + "^";
	result += this->HID + "^";
	result += this->SID + "^";

	return result;
}

void HelloMessage::deserialize(string data) {
	size_t found, start;
	string msg;

	start = 0;
	msg = data;

	found=msg.find("^", start);
  	if (found!=string::npos) {
  		this->AD = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

	found=msg.find("^", start);
  	if (found!=string::npos) {
  		this->HID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	found=msg.find("^", start);
  	if (found!=string::npos) {
  		this->SID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
}

AdvertisementMessage::AdvertisementMessage(){};
AdvertisementMessage::~AdvertisementMessage(){};

string AdvertisementMessage::serialize() const{
	string result;

	result += this->senderHID + "^";
	result += this->currSenderHID + "^";
	result += to_string(this->seq) + "^";
	result += to_string(this->ttl) + "^";
	result += to_string(this->distance) + "^";
	
	result += to_string(this->newCIDs.size()) + "^";
	for(auto it = this->newCIDs.begin(); it != this->newCIDs.end(); it++){
		result += *it + "^";
	}

	result += to_string(this->delCIDs.size()) + "^";
	for(auto it = this->delCIDs.begin(); it != this->delCIDs.end(); it++){
		result += *it + "^";
	}

	return result;
}

void AdvertisementMessage::deserialize(string data){
	size_t found, start;
	string msg, senderHID, seq_str, ttl_str, distance_str, num_cids_str, cid_str;

	start = 0;
	msg = data;

	found = msg.find("^", start);
  	if (found != string::npos) {
  		this->senderHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}


	found = msg.find("^", start);
  	if (found != string::npos) {
  		this->currSenderHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		seq_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		this->seq = atoi(seq_str.c_str());
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		ttl_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		this->ttl = atoi(ttl_str.c_str());
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		distance_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		this->distance = atoi(distance_str.c_str());
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		num_cids_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		uint32_t num_cids = atoi(num_cids_str.c_str());
  		for (unsigned i = 0; i < num_cids; ++i) {
  			found = msg.find("^", start);
  			if (found != string::npos) {
  				cid_str = msg.substr(start, found-start);
  				start = found+1;  // forward the search point
  			  
  				this->newCIDs.insert(cid_str);
  			}
  		}
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		num_cids_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		uint32_t num_cids = atoi(num_cids_str.c_str());
  		for (unsigned i = 0; i < num_cids; ++i) {
  			found = msg.find("^", start);
  			if (found != string::npos) {
  				cid_str = msg.substr(start, found-start);
  				start = found+1;  // forward the search point

  				this->delCIDs.insert(cid_str);
  			}
  		}
  	}
}

NeighborInfo::NeighborInfo(){};
NeighborInfo::~NeighborInfo(){};

void help(const char *name) {
	printf("\nusage: %s [-l level] [-v] [-t] [-h hostname]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v          : log to the console as well as syslog\n");
	printf(" -h hostname : click device name (default=router0)\n");
	printf("\n");
	exit(0);
}

void config(int argc, char** argv) {
	int c;
	unsigned level = 3;
	int verbose = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "h:l:v")) != -1) {
		switch (c) {
			case 'h':
				hostname = strdup(optarg);
				break;
			case 'l':
				level = MIN(atoi(optarg), LOG_DEBUG);
				break;
			case 'v':
				verbose = LOG_PERROR;
				break;
			case '?':
			default:
				// Help Me!
				help(basename(argv[0]));
				break;
		}
	}

	if (!hostname){
		hostname = strdup(DEFAULT_NAME);
	}

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

double nextWaitTimeInSecond(double ratePerSecond){
	double currRand = (double)rand()/(double)RAND_MAX;
	double nextTime = -1*log(currRand)/ratePerSecond;	// next time in second
	return nextTime;	
}

int interfaceNumber(string xidType, string xid) {
	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			if ((r.xid).compare(xid) == 0) {
				return (int)(r.port);
			}
		}
	}
	return -1;
}

void getRouteEntries(string xidType, vector<XIARouteEntry> & result){
	if(result.size() != 0){
		result.clear();
	}

	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			if(strcmp(r.xid.c_str(), ROUTE_XID_DEFAULT) != 0){
				result.push_back(r);
			}
		}
	} else {
		syslog(LOG_ALERT, "unable to get routes from click (%d)", rc);
	}
}

void CIDAdvertisementHandler(int signum){
	UNUSED(signum);

	advertiseCIDs();

	double nextTimeInSecond = nextWaitTimeInSecond(CID_ADVERT_UPDATE_RATE_PER_SEC);
	signal(SIGALRM, CIDAdvertisementHandler);
	ualarm((int)ceil(1000000*nextTimeInSecond),0); 	
}

void broadcastHello(){
	HelloMessage msg;
	msg.AD = routeState.myAD;
	msg.HID = routeState.myHID;
	msg.SID = routeState.mySID;

	char buffer[HELLO_MAX_BUF_SIZE];
	int buflen;
	bzero(buffer, HELLO_MAX_BUF_SIZE);

	string msgStr = msg.serialize();
	strcpy (buffer, msgStr.c_str());
	buflen = strlen(buffer);

	// send the broadcast
	if(Xsendto(routeState.helloSock, buffer, buflen, 0, (struct sockaddr*)&routeState.ddag, sizeof(sockaddr_x)) < 0){
		printf("Xsendto broadcast failed\n");
	}
}

void advertiseCIDs(){
	int n;
	AdvertisementMessage msg;
	msg.senderHID = routeState.myHID;
	msg.currSenderHID = routeState.myHID;
	msg.seq = routeState.lsaSeq;
	msg.ttl = MAX_TTL;
	msg.distance = 0;

	vector<XIARouteEntry> routeEntries;
	getRouteEntries("CID", routeEntries);

	set<string> currLocalCIDs;
	for(unsigned i = 0; i < routeEntries.size(); i++){
		if(routeEntries[i].port == (unsigned short)DESTINED_FOR_LOCALHOST){
			currLocalCIDs.insert(routeEntries[i].xid);
		}
	}
	
	// then find the deleted local CIDs
	for(auto it = routeState.localCIDs.begin(); it != routeState.localCIDs.end(); it++){
		if(currLocalCIDs.find(*it) == currLocalCIDs.end()){
			msg.delCIDs.insert(*it);
		}
	}

	// find all the new local CIDs first
	for(auto it = currLocalCIDs.begin(); it != currLocalCIDs.end(); it++){
		if(routeState.localCIDs.find(*it) == routeState.localCIDs.end()){
			msg.newCIDs.insert(*it);
		}
	}

	routeState.localCIDs = currLocalCIDs;

	// start advertise to each of my neighbors
	if(msg.delCIDs.size() > 0 || msg.newCIDs.size() > 0){
		// serialize the advertisement message
		string advertisement = msg.serialize();

		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			if (it->second.sendSock != -1 && (n = Xsend(it->second.sendSock, advertisement.c_str(), strlen(advertisement.c_str()), 0)) < 0) {
				printf("Xsend failed\n");
			}
		}

		routeState.lsaSeq = (routeState.lsaSeq + 1) % MAX_SEQNUM;
	}
}

void registerReceiver() {
	int sock;

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0){
		printf("Unable to create the listening socket\n");
		exit(-1);
	}

	if(XmakeNewSID(routeState.mySID, sizeof(routeState.mySID))) {
		printf("Unable to create a temporary SID\n");
		exit(-1);
	}

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, routeState.mySID, NULL, &ai) != 0){
		printf("getaddrinfo failure!\n");
		exit(-1);
	}

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;

	if (Xbind(sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(sock);
		printf("xbind failure!\n");
		exit(-1);	
	}

	Xlisten(sock, 5);

	routeState.masterSock = sock;
}

void initRouteState(){
    char cdag[MAX_DAG_SIZE];

	routeState.helloSock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (routeState.helloSock < 0) {
   		printf("Unable to create a socket\n");
   		exit(-1);
   	}

   	// read the localhost AD and HID
    if (XreadLocalHostAddr(routeState.helloSock, cdag, MAX_DAG_SIZE, routeState.my4ID, MAX_XID_SIZE) < 0 ){
        printf("Reading localhost address\n");
    	exit(0);
    }

    Graph g_localhost(cdag);
    strcpy(routeState.myAD, g_localhost.intent_AD_str().c_str());
    strcpy(routeState.myHID, g_localhost.intent_HID_str().c_str());

	// make the src DAG (the one the routing process listens on)
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_XCIDROUTE, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&routeState.sdag, ai->ai_addr, sizeof(sockaddr_x));

	// bind to the src DAG
   	if (Xbind(routeState.helloSock, (struct sockaddr*)&routeState.sdag, sizeof(sockaddr_x)) < 0) {
   		Graph g(&routeState.sdag);
   		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(routeState.helloSock);
   		exit(-1);
   	}

   	srand(time(NULL));
   	routeState.lsaSeq = 0;

   	Graph g = Node() * Node(BHID) * Node(SID_XCIDROUTE);
	g.fill_sockaddr(&routeState.ddag);

   	// wait sending the CID advertisement first
   	//signal(SIGALRM, CIDAdvertisementHandler);
	//ualarm((int)ceil(INIT_WAIT_TIME_SEC*1000000),0); 	
}

int connectToNeighbor(string AD, string HID, string SID){
	cout << "connect to neighbor with AD, HID and SID " << AD << " " << HID << " " << SID << endl;

	int sock = -1;
	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0){
		printf("Unable to create the listening socket\n");
		return -1;
	}

	sockaddr_x dag;
	socklen_t daglen = sizeof(sockaddr_x);

	Graph g = Node() * Node(AD) * Node(HID) * Node(SID);
	g.fill_sockaddr(&dag);

	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		printf("Unable to connect to the neighbor dag: %s\n", g.dag_string().c_str());
		return -1;
	}

	cout << "Xconnect sent to connect to" << g.dag_string() << endl;

	return sock;
}

void printNeighborInfo(){
	cout << "neighbor info: " << endl;
	for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
		cout << "\t AD:" << it->second.AD << endl;
		cout << "\t HID:" << it->second.HID << endl;
		cout << "\t port:" << it->second.port << endl;
		cout << "\t sendSock: " << it->second.sendSock << endl;
		cout << "\t recvSock: " << it->second.recvSock << endl;
	}
}

void processHelloMessage(){
	int n, sock, interface;
    char recvMessage[HELLO_MAX_BUF_SIZE];
    sockaddr_x theirDAG;
    socklen_t dlen;

    dlen = sizeof(sockaddr_x);

	n = Xrecvfrom(routeState.helloSock, recvMessage, HELLO_MAX_BUF_SIZE, 0, (struct sockaddr*)&theirDAG, &dlen);	
	if (n < 0) {
		printf("Xrecvfrom failed\n");
	} else {
		HelloMessage msg;
		msg.deserialize(recvMessage);

		string key = msg.AD + msg.HID;
		if(routeState.neighbors[key].sendSock == -1) {
			sock = connectToNeighbor(msg.AD, msg.HID, msg.SID);
			if(sock == -1){
				return;
			}
			
			interface = interfaceNumber("HID", msg.HID);
		
			routeState.neighbors[key].AD = msg.AD;
			routeState.neighbors[key].HID = msg.HID;
			routeState.neighbors[key].port = interface;
			routeState.neighbors[key].sendSock = sock;
		}
	}
}

void processNeighborJoin(){
	int32_t acceptSock = -1;
	string AD, HID;
	sockaddr_x addr;
	socklen_t daglen;
	daglen = sizeof(addr);

	if ((acceptSock = Xaccept(routeState.masterSock, (struct sockaddr*)&addr, &daglen)) < 0){
		printf("Xaccept failed\n");
		return;
	}

	Graph g(&addr);
	// find the AD and HID of the peer
	for(int i = 0; i < g.num_nodes(); i++){
		Node currNode = g.get_node(i);

		if(currNode.type_string() == Node::XID_TYPE_AD_STRING){
            AD = currNode.to_string();
        } else if (currNode.type_string() == Node::XID_TYPE_HID_STRING){
            HID = currNode.to_string();
        }
	}

	int	interface = interfaceNumber("HID", HID);
	string key = AD + HID;

	routeState.neighbors[key].recvSock = acceptSock;
	routeState.neighbors[key].AD = AD;
	routeState.neighbors[key].HID = HID;
	routeState.neighbors[key].port = interface;
}

void processNeighborMessage(const NeighborInfo &neighbor){
	int n;
	char recvMessage[CID_MAX_BUF_SIZE];

	if ((n = Xrecv(neighbor.recvSock, recvMessage, CID_MAX_BUF_SIZE, 0))  < 0) {
		printf("Xrecv failed\n");
	}

	// deseralize the message
	AdvertisementMessage msg;
	msg.deserialize(recvMessage);

	// check the if the seq number is valid
	if(msg.seq < routeState.HID2Seq[msg.senderHID] 
			&& routeState.HID2Seq[msg.senderHID] - msg.seq < 100000){
		return;
	}
	routeState.HID2Seq[msg.senderHID] = msg.seq;

	// remove the entries that need to be removed
	for(auto it = msg.delCIDs.begin(); it != msg.delCIDs.end(); it++){
		if(routeState.CIDRoutes.find(*it) != routeState.CIDRoutes.end()){
			CIDRouteEntry currEntry = routeState.CIDRoutes[*it];

			// remove an entry if it is from the same host
			if(currEntry.dest == msg.senderHID){
				routeState.CIDRoutes.erase(*it);
				xr.delRouteCIDRouting(*it);
			}
		}
	}

	// then check for each CID if it is the closest for current router
	set<string> advertiseAddition;
	for(auto it = msg.newCIDs.begin(); it != msg.newCIDs.end(); it++){
		string currNewCID = *it;

		// if the CID is not stored locally
		if(routeState.localCIDs.find(currNewCID) == routeState.localCIDs.end()){
			advertiseAddition.insert(currNewCID);
			// and the CID is not encountered before or it is encountered before 
			// but the distance is longer then
			if(routeState.CIDRoutes.find(currNewCID) == routeState.CIDRoutes.end() 
				|| routeState.CIDRoutes[currNewCID].cost > msg.distance){
				
				routeState.CIDRoutes[currNewCID].cost = msg.distance;
				routeState.CIDRoutes[currNewCID].port = neighbor.port;
				routeState.CIDRoutes[currNewCID].nextHop = neighbor.HID;
				routeState.CIDRoutes[currNewCID].dest = msg.senderHID;
				xr.setRouteCIDRouting(currNewCID, neighbor.port, neighbor.HID, 0xffff);
			}
		}
	}

	// update the message and broadcast to other neighbor
	if(msg.ttl > 0){
		AdvertisementMessage msg2Others;
		msg2Others.senderHID = msg.senderHID;
		msg2Others.currSenderHID = routeState.myHID;
		msg2Others.seq = msg.seq;
		msg2Others.ttl = msg.ttl - 1;
		msg2Others.distance = msg.distance + 1;
		msg2Others.newCIDs = advertiseAddition;
		msg2Others.delCIDs = msg.delCIDs;

		string msg2OthersStr = msg2Others.serialize();

		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			if(it->second.HID != neighbor.HID){

				if (it->second.sendSock != -1 && (n = Xsend(it->second.sendSock, msg2OthersStr.c_str(), strlen(msg2OthersStr.c_str()), 0)) < 0) {
					printf("Xsend failed\n");
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	int rc, selectRetVal, iteration = 0;
	int32_t highSock;
    struct timeval timeoutval;
    fd_set socks;

	// config helper
	config(argc, argv);

	// connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click (%d)", rc);
		return -1;
	}
	xr.setRouter(hostname);

	initRouteState();
   	registerReceiver();

   	// broadcast hello first
   	broadcastHello();

	while(1){
		iteration++;
		if(iteration % 400 == 0){
			broadcastHello();
		}

		FD_ZERO(&socks);
		FD_SET(routeState.helloSock, &socks);
		FD_SET(routeState.masterSock, &socks);

		highSock = max(routeState.helloSock, routeState.masterSock);
		/*
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			if(it->second.recvSock > highSock){
				highSock = it->second.recvSock;
			}

			if(it->second.recvSock != -1){
				FD_SET(it->second.recvSock, &socks);
			}
		}
		 */
		
		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000;

		selectRetVal = Xselect(highSock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			if (FD_ISSET(routeState.helloSock, &socks)) {
				// if we received a hello packet from neighbor, check if it is a new neighbor
				// and then establish connection state with them
				processHelloMessage();
			} else if (FD_ISSET(routeState.masterSock, &socks)) {
				// if a new neighbor connects, add to the recv list
				processNeighborJoin();
			} else {
				/*
				for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
					// if we recv a message from one of our established neighbor, procees the message
					if(it->second.recvSock != -1 && FD_ISSET(it->second.recvSock, &socks)){
						processNeighborMessage(it->second);
					}
				}
				 */
			}

			printNeighborInfo();
		}
	}

	return 0;
}