#include "xcidrouted.h"

using namespace std;

static int ttl = -1;
static char *ident = NULL;
static char* hostname = NULL;

static XIARouter xr;
static RouteState routeState;

#if defined(STATS_LOG)
static Logger* logger;
#endif

// should CID routing handle topology changes?
// 
// routing uses hard state advertisement to avoid advertisement
// scalability issues. But the use of hard state also means that 
// if in-network state fails, the routing would also fail to achieve
// its purpose. More concretely, topology impact routing decisions and if 
// topology changes, routing decision would be stale. So the best we can
// do is when this happens, CID routing should start over (using soft
// state).

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

void HelloMessage::print() const{
	printf("HelloMessage: \n");
	printf("\tAD: %s HID %s SID %s\n", this->AD.c_str(), this->HID.c_str(), this->SID.c_str());
}

int HelloMessage::send(){
	char buffer[HELLO_MAX_BUF_SIZE];
	int buflen;
	bzero(buffer, HELLO_MAX_BUF_SIZE);

	string msgStr = serialize();
	strcpy (buffer, msgStr.c_str());
	buflen = strlen(buffer);

	// send the broadcast
	if(Xsendto(routeState.helloSock, buffer, buflen, 0, (struct sockaddr*)&routeState.ddag, sizeof(sockaddr_x)) < 0){
		printf("HelloMessage: Xsendto broadcast failed\n");
		return -1;
	}

	return 1;
}

int HelloMessage::recv(){
	int n;
	char recvMessage[HELLO_MAX_BUF_SIZE];
    sockaddr_x theirDAG;
    socklen_t dlen;

    dlen = sizeof(sockaddr_x);
	n = Xrecvfrom(routeState.helloSock, recvMessage, HELLO_MAX_BUF_SIZE, 0, (struct sockaddr*)&theirDAG, &dlen);	
	if (n < 0) {
		printf("Xrecvfrom failed\n");
		return -1;
	} else {
		deserialize(recvMessage);
	}

	return 1;
}

AdvertisementMessage::AdvertisementMessage(){};
AdvertisementMessage::~AdvertisementMessage(){};

string AdvertisementMessage::serialize() const{
	string result;

	result += to_string(CIDMessage::Advertise) + "^";
	result += this->info.senderHID + "^";
	result += to_string(this->info.ttl) + "^";
	result += to_string(this->info.distance) + "^";
	
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
	string msg, senderHID, ttl_str, distance_str, num_cids_str, cid_str;

	start = 0;
	msg = data;

	found = msg.find("^", start);
  	if (found != string::npos) {
  		this->info.senderHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		ttl_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		this->info.ttl = atoi(ttl_str.c_str());
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		distance_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		this->info.distance = atoi(distance_str.c_str());
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

void AdvertisementMessage::print() const{
	printf("AdvertisementMessage: \n");
	printf("\tsenderHID: %s\n", this->info.senderHID.c_str());
	printf("\tttl: %d\n", this->info.ttl);
	printf("\tdistance: %d\n", this->info.distance);

	printf("\tnewCIDs:\n");
	for(auto it = this->newCIDs.begin(); it != this->newCIDs.end(); it++){
		printf("\t\t%s\n", it->c_str());
	}

	printf("\tdelCIDs:\n");
	for(auto it = this->delCIDs.begin(); it != this->delCIDs.end(); it++){
		printf("\t\t%s\n", it->c_str());
	}
}

int AdvertisementMessage::send(int sock){
	return sendMessageToSock(sock, serialize());
}

NodeJoinMessage::NodeJoinMessage(){};
NodeJoinMessage::~NodeJoinMessage(){};

string NodeJoinMessage::serialize() const{
	string result;

	result += to_string(CIDMessage::Join) + "^";

	result += to_string(this->CID2Info.size()) + "^";
	for(auto it = this->CID2Info.begin(); it != this->CID2Info.end(); ++it){
		result += it->first + "^";
		result += to_string(it->second.ttl) + "^";
		result += to_string(it->second.distance) + "^";
		result += it->second.senderHID + "^";
	}

	return result;
}

void NodeJoinMessage::deserialize(string data) {
	size_t found, start;
	string msg, num_info_str, cid_str, ttl_str, distance_str, dest_str;

	start = 0;
	msg = data;

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		num_info_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	int num_info = atoi(num_info_str.c_str());

  	for(int i = 0; i < num_info; i++){
  		found = msg.find("^", start);
  		if (found != string::npos) {
  			cid_str = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		found = msg.find("^", start);
  		if (found != string::npos) {
  			ttl_str = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		int ttl = atoi(ttl_str.c_str());

  		found = msg.find("^", start);
  		if (found != string::npos) {
  			distance_str = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		int distance = atoi(distance_str.c_str());

  		found = msg.find("^", start);
  		if (found != string::npos) {
  			dest_str = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		this->CID2Info[cid_str].ttl = ttl;
  		this->CID2Info[cid_str].distance = distance;
  		this->CID2Info[cid_str].senderHID = dest_str;
  	}
}

void NodeJoinMessage::print() const {
	printf("NodeJoinMessage: \n");

	printf("\tCID2Info:\n");
	for(auto it = this->CID2Info.begin(); it != this->CID2Info.end(); ++it){
		printf("\t\tCID: %s\n", it->first.c_str());
		printf("\t\tttl: %u\n", it->second.ttl);
		printf("\t\tdistance: %u\n", it->second.distance);
		printf("\t\tdestHID: %s\n", it->second.senderHID.c_str());
	}
}

int NodeJoinMessage::send(int sock){
	return sendMessageToSock(sock, serialize());
}

NodeLeaveMessage::NodeLeaveMessage(){};
NodeLeaveMessage::~NodeLeaveMessage(){};

string NodeLeaveMessage::serialize() const{
	string result;

	result += to_string(CIDMessage::Leave) + "^";
	result += this->prevHID + "^";

	result += to_string(this->CID2Info.size()) + "^";
	for(auto it = this->CID2Info.begin(); it != this->CID2Info.end(); ++it){
		result += it->first + "^";
		result += to_string(it->second.ttl) + "^";
		result += to_string(it->second.distance) + "^";
		result += it->second.senderHID + "^";
	}

	return result;
}

void NodeLeaveMessage::deserialize(string data) {
	size_t found, start;
	string msg, num_info_str, cid_str, ttl_str, distance_str, dest_str;

	start = 0;
	msg = data;

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		this->prevHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		num_info_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	int num_info = atoi(num_info_str.c_str());
  	for(int i = 0; i < num_info; i++){
  		found = msg.find("^", start);
  		if (found != string::npos) {
  			cid_str = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		found = msg.find("^", start);
  		if (found != string::npos) {
  			ttl_str = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		int ttl = atoi(ttl_str.c_str());

  		found = msg.find("^", start);
  		if (found != string::npos) {
  			distance_str = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		int distance = atoi(ttl_str.c_str());

  		found = msg.find("^", start);
  		if (found != string::npos) {
  			dest_str = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		this->CID2Info[cid_str].ttl = ttl;
  		this->CID2Info[cid_str].distance = distance;
  		this->CID2Info[cid_str].senderHID = dest_str;
  	}
}

void NodeLeaveMessage::print() const {
	printf("NodeLeaveMessage: \n");
	printf("\tprevHID: %s\n", this->prevHID.c_str());

	printf("\tCID2TTL:\n");
	for(auto it = this->CID2Info.begin(); it != this->CID2Info.end(); ++it){
		printf("\t\tCID: %s\n", it->first.c_str());
		printf("\t\tttl: %u\n", it->second.ttl);
		printf("\t\tdistance: %u\n", it->second.distance);
		printf("\t\tsenderHID: %s\n", it->second.senderHID.c_str());
	}
}

int NodeLeaveMessage::send(int sock){
	return sendMessageToSock(sock, serialize());
}

NeighborInfo::NeighborInfo(){};
NeighborInfo::~NeighborInfo(){};

void help(const char *name) {
	printf("\nusage: %s [-l level] [-v] [-t] [-h hostname] [-t TTL]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v          : log to the console as well as syslog\n");
	printf(" -h hostname : click device name (default=router0)\n");
	printf(" -t TTL    	 : TTL for the CID advertisement, default is 1\n");
	printf("\n");
	exit(0);
}

void config(int argc, char** argv) {
	int c;
	unsigned level = 3;
	int verbose = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "h:l:v:t:")) != -1) {
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
			case 't':
				ttl = atoi(optarg);
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

	if(ttl <= 0 || ttl > MAX_TTL){
		ttl = MAX_TTL;
	}

	printf("ttl is: %d\n", ttl);

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

void cleanup(int) {
#if defined(STATS_LOG)
	logger->end();
	delete logger;
#endif

	routeState.mtx.lock();
	for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
		Xclose(it->second.recvSock);
		Xclose(it->second.sendSock);
	}
	routeState.mtx.unlock();

	exit(1);
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
		syslog(LOG_ALERT, "unable to get routes from click (%d) %s", rc, xr.cserror());
	}
}

size_t getTotalBytesForCIDRoutes(){
	size_t result = 0;

	for(auto it = routeState.localCIDs.begin(); it != routeState.localCIDs.end(); ++it){
		result += it->size();
	}

#ifdef FILTER
	for(auto it = routeState.CIDRoutesWithFilter.begin(); it != routeState.CIDRoutesWithFilter.end(); ++it){
		result += it->first.size();
		result += it->second.nextHop.size();
		result += it->second.dest.size();
		result += sizeof(it->second.port) + sizeof(it->second.cost);
	}
#else
	for(auto it = routeState.CIDRoutes.begin(); it != routeState.CIDRoutes.end(); ++it){
		result += it->first.size();
		for(auto ij = it->second.begin(); ij != it->second.end(); ++ij){
			result += ij->first.size();
			
			result += ij->second.nextHop.size();
			result += ij->second.dest.size();
			result += sizeof(it->second.port) + sizeof(it->second.cost);
		}
	}
#endif

	return result;
}

void CIDAdvertiseTimer(){
	thread([](){
        while (true) {
            double nextTimeInSecond = nextWaitTimeInSecond(CID_ADVERT_UPDATE_RATE_PER_SEC);
            int nextTimeMilisecond = (int) ceil(nextTimeInSecond * 1000);

            advertiseCIDs();

#ifdef STATS_LOG
            routeState.mtx.lock();
			size_t totalSize = getTotalBytesForCIDRoutes();
			logger->log("size " + to_string(totalSize));
			routeState.mtx.unlock();
#endif

            this_thread::sleep_for(chrono::milliseconds(nextTimeMilisecond));
        }
    }).detach();
}

#ifdef FILTER
// only clean CID routes if we are doing filtering since we might want to keep some routes
// in case our local cids are evicted
void cleanCIDRoutes(){
	set<string> toRemove;
	for(auto it = routeState.CIDRoutesWithFilter.begin(); it != routeState.CIDRoutesWithFilter.end(); it++){
		if(routeState.localCIDs.find(it->first) != routeState.localCIDs.end()){
			toRemove.insert(it->first);
		}
	}

	for(auto it = toRemove.begin(); it != toRemove.end(); it++){
		routeState.CIDRoutesWithFilter.erase(*it);
	}
}
#else

void setMinCostCIDRoutes(string cid){
	if(routeState.CIDRoutes.find(cid) != routeState.CIDRoutes.end()){
		uint32_t minCost = INT_MAX;
		CIDRouteEntry minCostEntry;

		for(auto it = routeState.CIDRoutes[cid].begin(); it != routeState.CIDRoutes[cid].end(); it++){
			if(it->second.cost < minCost){
				minCost = it->second.cost;
				minCostEntry = it->second;
			}
		}
		xr.setRouteCIDRouting(cid, minCostEntry.port, minCostEntry.nextHop, 0xffff);
	}
}

void resetNonLocalCIDRoutes(const set<string> & delLocal){
	for(auto it = delLocal.begin(); it != delLocal.end(); it++){
		string currDelLocal = *it;
		// if we have the cid routes for local CID routes that have just been removed
		// set the shortest of them.
		setMinCostCIDRoutes(currDelLocal);
	}
}
#endif

void advertiseCIDs(){
	printf("advertising CIDs...\n");

	AdvertisementMessage msg;
	msg.info.senderHID = routeState.myHID;
	msg.info.ttl = ttl;
	msg.info.distance = 0;

	// C++ socket is generally not thread safe. So talking to click here means we 
	// need to lock it.
	routeState.mtx.lock();
	vector<XIARouteEntry> routeEntries;
	getRouteEntries("CID", routeEntries);
	routeState.mtx.unlock();

	set<string> currLocalCIDs;
	for(unsigned i = 0; i < routeEntries.size(); i++){
		if(routeEntries[i].port == (unsigned short)DESTINED_FOR_LOCALHOST){
			currLocalCIDs.insert(routeEntries[i].xid);
		}
	}

	// then find the deleted local CIDs
	for(auto it = routeState.localCIDs.begin(); it != routeState.localCIDs.end(); it++){
		if(currLocalCIDs.find(*it) == currLocalCIDs.end()){
			printf("sending delete CID message: %s\n", it->c_str());
			msg.delCIDs.insert(*it);
		}
	}

	// find all the new local CIDs first
	for(auto it = currLocalCIDs.begin(); it != currLocalCIDs.end(); it++){
		if(routeState.localCIDs.find(*it) == routeState.localCIDs.end()){
			printf("sending new CID message: %s\n", it->c_str());
			msg.newCIDs.insert(*it);
		}
	}

	routeState.mtx.lock();
	routeState.localCIDs = currLocalCIDs;

#ifdef FILTER
	cleanCIDRoutes();
#else
	resetNonLocalCIDRoutes(msg.delCIDs);
#endif
	routeState.mtx.unlock();

	// start advertise to each of my neighbors
	if(msg.delCIDs.size() > 0 || msg.newCIDs.size() > 0){
		routeState.mtx.lock();

		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			printf("sending CID advertisement to neighbor%s\n", it->first.c_str());
			msg.send(it->second.sendSock);
		}

		routeState.mtx.unlock();
	}

	printf("done\n");
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

   	Graph g = Node() * Node(BHID) * Node(SID_XCIDROUTE);
	g.fill_sockaddr(&routeState.ddag);

	// remove any previously set route and start fresh.
	vector<XIARouteEntry> routeEntries;
	getRouteEntries("CID", routeEntries);
	for(unsigned i = 0; i < routeEntries.size(); i++){
		if(routeEntries[i].port != (unsigned short)DESTINED_FOR_LOCALHOST){
			xr.delRouteCIDRouting(routeEntries[i].xid);
		}
	}
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
	HelloMessage msg;
	msg.recv();

	string key = msg.AD + msg.HID;

	if(routeState.neighbors[key].sendSock == -1) {
		int sock = connectToNeighbor(msg.AD, msg.HID, msg.SID);
		if(sock == -1){
			return;
		}

		routeState.mtx.lock();
		int interface = interfaceNumber("HID", msg.HID);
		routeState.neighbors[key].AD = msg.AD;
		routeState.neighbors[key].HID = msg.HID;
		routeState.neighbors[key].port = interface;
		routeState.neighbors[key].sendSock = sock;
		routeState.neighbors[key].timer = time(NULL);
		// send to new neighbor everything we know so far about the CID routes
		sendNeighborJoin(routeState.neighbors[key]);
		routeState.mtx.unlock();
	} else {
		routeState.neighbors[key].timer = time(NULL);
	}
}

void removeExpiredNeighbor(string neighbor){
	auto it = routeState.neighbors.find(neighbor);

	if(it != routeState.neighbors.end()){
		if(routeState.neighbors[neighbor].sendSock != -1){
			Xclose(routeState.neighbors[neighbor].sendSock);
			routeState.neighbors[neighbor].sendSock = -1;
		}
		if(routeState.neighbors[neighbor].recvSock != -1){
			Xclose(routeState.neighbors[neighbor].recvSock);
			routeState.neighbors[neighbor].recvSock = -1;
		}

		printf("erasing: %s\n", neighbor.c_str());
		routeState.neighbors.erase(it);
	}
}

void removeExpiredNeighbors(vector<string> neighbors){
	routeState.mtx.lock();
	// first remove the unused neighbors
	for(auto it = neighbors.begin(); it != neighbors.end(); ++it){
		removeExpiredNeighbor(*it);
	}

	// then remove routes
	for(auto it = neighbors.begin(); it != neighbors.end(); ++it){
		NeighborInfo currNeighbor = routeState.neighbors[*it];
		printf("neighbor: %s has expired\n", it->c_str());
		sendNeighborLeave(currNeighbor);
	}
	routeState.mtx.unlock();
}

void checkExpiredNeighbors(){
	time_t now = time(NULL);

	printNeighborInfo();

	vector<string> candidates;
	for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); ++it){
		// if this neighbor is expired
		if(now - it->second.timer >= HELLO_EXPIRE){
			candidates.push_back(it->second.AD+it->second.HID);
		}
	}

	removeExpiredNeighbors(candidates);
}

void processNeighborConnect(){
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

	string key = AD + HID;

	routeState.mtx.lock();
	int	interface = interfaceNumber("HID", HID);
	routeState.neighbors[key].recvSock = acceptSock;
	routeState.neighbors[key].AD = AD;
	routeState.neighbors[key].HID = HID;
	routeState.neighbors[key].port = interface;
	routeState.mtx.unlock();
	routeState.neighbors[key].timer = time(NULL);

}

// function already protected by locks
void sendNeighborJoin(const NeighborInfo &neighbor){

#ifdef FILTER
	NodeJoinMessage msg;

	for(auto it = routeState.CIDRoutesWithFilter.begin(); it != routeState.CIDRoutesWithFilter.end(); ++it){
		string currCID = it->first;
		string currCIDDest = it->second.dest;
		uint32_t currCIDCost = it->second.cost;

		if(ttl - currCIDCost - 1 > 0){
			msg.CID2Info[currCID].ttl = ttl - currCIDCost - 1;
			msg.CID2Info[currCID].distance = currCIDCost + 1;
			msg.CID2Info[currCID].senderHID = currCIDDest;
		}
	}

	for(auto it = routeState.localCIDs.begin(); it != routeState.localCIDs.end(); ++it){
		msg.CID2Info[*it].ttl = ttl;
		msg.CID2Info[*it].distance = 0;
		msg.CID2Info[*it].senderHID = routeState.myHID;
	}

	if(msg.CID2Info.size() > 0){
		printf("send neighbor join to %s\n", neighbor.HID.c_str());
		msg.send(neighbor.sendSock);
	}

#else
	//TODO: non-filter version of this
#endif
}

// function already protected by locks
void sendNeighborLeave(const NeighborInfo &neighbor){
#ifdef FILTER
	NodeLeaveMessage msg;

	for(auto it = routeState.CIDRoutesWithFilter.begin(); it != routeState.CIDRoutesWithFilter.end(); ++it){
		string currCID = it->first;
		string currCIDDest = it->second.dest;
		string currCIDNextHop = it->second.nextHop;
		uint32_t currCIDCost = it->second.cost;

		if(currCIDNextHop == neighbor.HID && ttl - currCIDCost - 1 > 0){
			msg.CID2Info[currCID].ttl = ttl - currCIDCost - 1;
			msg.CID2Info[currCID].distance = currCIDCost + 1;
			msg.CID2Info[currCID].senderHID = currCIDDest;

			// delete routes to the neighbor
			xr.delRouteCIDRouting(currCID);
		}
	}

	if(msg.CID2Info.size() > 0){
		printf("send neighbor leave from neighbor: %s\n", neighbor.HID.c_str());
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); ++it){
			msg.send(it->second.sendSock);
		}
	}

#else
	//TODO: non-filter version of this
#endif
}

/**
 * Tradeoff between filtering and not-filtering
 * 	filtering: less traffic to propagate especially when redundancy between upstream and downstream router
 * 		is high. But lose valuable information: router might want to keep information of multiple routes
 * 	 	to reach CID
 *
 * 	not-filtering: more traffic; more information available at each router. During route eviction, router can 
 * 		use these information to replace with another longer/equal cost route.
 */

#ifdef FILTER

set<string> deleteCIDRoutesWithFilter(const AdvertisementMessage & msg){
	set<string> routeDeletion;
	for(auto it = msg.delCIDs.begin(); it != msg.delCIDs.end(); it++){
		string currDelCID = *it;

		if(routeState.CIDRoutesWithFilter.find(currDelCID) != routeState.CIDRoutesWithFilter.end()){
			CIDRouteEntry currEntry = routeState.CIDRoutesWithFilter[currDelCID];

			// remove an entry only if it is from the same host and follows the same path
			if(currEntry.dest == msg.info.senderHID && currEntry.cost == msg.info.distance){
				routeDeletion.insert(currDelCID);
#ifdef STATS_LOG
				logger->log("route deletion");
#endif
				routeState.CIDRoutesWithFilter.erase(currDelCID);
				xr.delRouteCIDRouting(currDelCID);
			}
		}

	}

	return routeDeletion;
}

set<string> setCIDRoutesWithFilter(const AdvertisementMessage & msg, const NeighborInfo &neighbor){
	set<string> routeAddition;
	for(auto it = msg.newCIDs.begin(); it != msg.newCIDs.end(); it++){
		string currNewCID = *it;

		// if the CID is not stored locally
		if(routeState.localCIDs.find(currNewCID) == routeState.localCIDs.end()){
			// and the CID is not encountered before or it is encountered before 
			// but the distance is longer then
			if(routeState.CIDRoutesWithFilter.find(currNewCID) == routeState.CIDRoutesWithFilter.end() 
				|| routeState.CIDRoutesWithFilter[currNewCID].cost > msg.info.distance){
				routeAddition.insert(currNewCID);

#ifdef STATS_LOG
				logger->log("route addition");
#endif

				// set corresponding CID route entries
				routeState.CIDRoutesWithFilter[currNewCID].cost = msg.info.distance;
				routeState.CIDRoutesWithFilter[currNewCID].port = neighbor.port;
				routeState.CIDRoutesWithFilter[currNewCID].nextHop = neighbor.HID;
				routeState.CIDRoutesWithFilter[currNewCID].dest = msg.info.senderHID;
				xr.setRouteCIDRouting(currNewCID, neighbor.port, neighbor.HID, 0xffff);
			}
		}
	}
	return routeAddition;
}

#else

// non-filtering code
void deleteCIDRoutes(const AdvertisementMessage & msg){
	// for each CID routes in the message that would need to be removed
	for(auto it = msg.delCIDs.begin(); it != msg.delCIDs.end(); it++){
		string currDelCID = *it;

		// if CID route entries has this CID from this message sender
		if(routeState.CIDRoutes.find(currDelCID) != routeState.CIDRoutes.end() && 
			routeState.CIDRoutes[currDelCID].find(msg.info.senderHID) != routeState.CIDRoutes[currDelCID].end()){
			CIDRouteEntry currEntry = routeState.CIDRoutes[currDelCID][msg.info.senderHID];

			// remove the accounting since this seneder just send a delete message
			// for this CID
			routeState.CIDRoutes[currDelCID].erase(msg.info.senderHID);

			if(routeState.CIDRoutes[currDelCID].size() == 0){
#ifdef STATS_LOG
				logger->log("route deletion");
#endif
				// if no more CID routes left for this CID from other sender, just remove the CID routes
				xr.delRouteCIDRouting(currDelCID);
			} else {
				// if there are CID routes left for this CID from other sender, just pick one with the minimum 
				// distance and set it. We can do it since we haven't received eviction message from other sender
				setMinCostCIDRoutes(currDelCID);
			}
		}
	}
}

void setCIDRoutes(const AdvertisementMessage & msg, const NeighborInfo &neighbor){
	// then check for each CID if it is the closest for the current router
	for(auto it = msg.newCIDs.begin(); it != msg.newCIDs.end(); it++){
		string currNewCID = *it;
		// and the CID is not encountered before or it is encountered before 
		// but the distance is longer then
		if(routeState.CIDRoutes.find(currNewCID) == routeState.CIDRoutes.end()) {
			// set corresponding CID route entries
			routeState.CIDRoutes[currNewCID][msg.info.senderHID].cost = msg.distance;
			routeState.CIDRoutes[currNewCID][msg.info.senderHID].port = neighbor.port;
			routeState.CIDRoutes[currNewCID][msg.info.senderHID].nextHop = neighbor.HID;
			routeState.CIDRoutes[currNewCID][msg.info.senderHID].dest = msg.info.senderHID;
			
#ifdef STATS_LOG
			logger->log("route addition");
#endif

			xr.setRouteCIDRouting(currNewCID, neighbor.port, neighbor.HID, 0xffff);
		} else {
			uint32_t minDist = INT_MAX;
			for(auto jt = routeState.CIDRoutes[currNewCID].begin(); jt != routeState.CIDRoutes[currNewCID].end(); jt++){
				if(jt->second.cost < minDist){
					minDist = jt->second.cost;
				}
			}

			// only set the routes if current message is the shortest to reach the CID
			if(minDist > msg.distance){
#ifdef STATS_LOG
				logger->log("route addition");
#endif
				xr.setRouteCIDRouting(currNewCID, neighbor.port, neighbor.HID, 0xffff);
			}

			if(routeState.CIDRoutes[currNewCID].find(msg.info.senderHID) == routeState.CIDRoutes[currNewCID].end() 
				|| routeState.CIDRoutes[currNewCID][msg.info.senderHID].cost > msg.distance){
				routeState.CIDRoutes[currNewCID][msg.info.senderHID].cost = msg.distance;
				routeState.CIDRoutes[currNewCID][msg.info.senderHID].port = neighbor.port;
				routeState.CIDRoutes[currNewCID][msg.info.senderHID].nextHop = neighbor.HID;
				routeState.CIDRoutes[currNewCID][msg.info.senderHID].dest = msg.info.senderHID;
			}
		}
	}
}

#endif

int handleAdvertisementMessage(string data, const NeighborInfo &neighbor){
	AdvertisementMessage msg;
	msg.deserialize(data);

	printf("Received message from neighbor %s\n", neighbor.HID.c_str());
	msg.print();

	// our communication to XIA writes to single socket and is
	// not thread safe.
	routeState.mtx.lock();

#ifdef FILTER
	set<string> routeAddition = setCIDRoutesWithFilter(msg, neighbor);
	set<string> routeDeletion = deleteCIDRoutesWithFilter(msg);
#else
	deleteCIDRoutes(msg);
	setCIDRoutes(msg, neighbor);
#endif
	
	routeState.mtx.unlock();

	// update the message and broadcast to other neighbor
	// 	iff there are something meaningful to broadcast
	// 	AND ttl is not going to be zero
#ifdef FILTER
	if(msg.info.ttl - 1 > 0 && (routeAddition.size() > 0 || routeDeletion.size() > 0)){
#else
	if(msg.info.ttl - 1 > 0 && (msg.newCIDs.size() > 0 || msg.delCIDs.size() > 0)){
#endif
		AdvertisementMessage msg2Others;
		msg2Others.info.senderHID = msg.info.senderHID;
		msg2Others.info.ttl = msg.info.ttl - 1;
		msg2Others.info.distance = msg.info.distance + 1;

#ifdef FILTER
		msg2Others.newCIDs = routeAddition;
		msg2Others.delCIDs = routeDeletion;
#else
		msg2Others.newCIDs = msg.newCIDs;
		msg2Others.delCIDs = msg.delCIDs;
#endif

		routeState.mtx.lock();
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			if(it->second.HID != neighbor.HID && msg.info.senderHID != neighbor.HID){
				msg2Others.send(it->second.sendSock);
			}
		}
		routeState.mtx.unlock();
	}

	return 0;
}

// process the node join message from this neighbor
int handleNodeJoinMessage(string data, const NeighborInfo &neighbor){
	NodeJoinMessage msg;
	msg.deserialize(data);

#ifdef FILTER
	routeState.mtx.lock();

	vector<string> candidates;
	for(auto it = msg.CID2Info.begin(); it != msg.CID2Info.end(); ++it){
		string currCID = it->first;
		string currCIDDestHID = it->second.senderHID;
		uint32_t currCIDCost = it->second.distance;
		bool shouldSetRoute = false;
		
		if(routeState.CIDRoutesWithFilter.find(currCID) == routeState.CIDRoutesWithFilter.end() 
			|| routeState.CIDRoutesWithFilter[currCID].cost > currCIDCost){
			shouldSetRoute = true;
		}

		if(shouldSetRoute){
			routeState.CIDRoutesWithFilter[currCID].dest = currCIDDestHID;
			routeState.CIDRoutesWithFilter[currCID].nextHop = neighbor.HID;
			routeState.CIDRoutesWithFilter[currCID].port = neighbor.port;
			routeState.CIDRoutesWithFilter[currCID].cost = currCIDCost;
			xr.setRouteCIDRouting(currCID, neighbor.port, neighbor.HID, 0xffff);

			if(msg.CID2Info[currCID].ttl - 1 > 0){
				msg.CID2Info[currCID].ttl = msg.CID2Info[currCID].ttl - 1;
				msg.CID2Info[currCID].distance = msg.CID2Info[currCID].distance + 1;
			} else {
				candidates.push_back(currCID);
			}
		} else {
			candidates.push_back(currCID);
		}
	}

	for(auto it = candidates.begin(); it != candidates.end(); ++it){
		msg.CID2Info.erase(*it);
	}

	if(msg.CID2Info.size() != 0){
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); ++it){
			if(it->first != neighbor.HID){
				msg.send(it->second.sendSock);
			}
		}
	}

	routeState.mtx.unlock();

#else

#endif
	return 0;
}

// process the node leave message from this neighbor
int handleNodeLeaveMessage(string data, const NeighborInfo &neighbor){
	NodeLeaveMessage msg;
	msg.deserialize(data);

#ifdef FILTER
	routeState.mtx.lock();

	vector<string> candidates;
	for(auto it = msg.CID2Info.begin(); it != msg.CID2Info.end(); ++it){
		string currCID = it->first;
		string currCIDDestHID = it->second.senderHID;
		uint32_t currCIDCost = it->second.distance;

		if(routeState.CIDRoutesWithFilter.find(currCID) != routeState.CIDRoutesWithFilter.end() && 
			routeState.CIDRoutesWithFilter[currCID].nextHop == msg.prevHID &&
			routeState.CIDRoutesWithFilter[currCID].dest == currCIDDestHID &&
			routeState.CIDRoutesWithFilter[currCID].cost == currCIDCost){
			xr.delRouteCIDRouting(currCID);

			if(msg.CID2Info[currCID].ttl - 1 > 0){
				msg.CID2Info[currCID].ttl = msg.CID2Info[currCID].ttl - 1;
				msg.CID2Info[currCID].distance = msg.CID2Info[currCID].distance + 1;
			} else {
				candidates.push_back(it->first);
			}
		} else {
			candidates.push_back(it->first);
		}
	}

	for(auto it = candidates.begin(); it != candidates.end(); ++it){
		msg.CID2Info.erase(*it);
	}

	if(msg.CID2Info.size() != 0){
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); ++it){
			if(it->first != neighbor.HID){
				msg.send(it->second.sendSock);
			}
		}		
	}

	routeState.mtx.unlock();
#else

#endif

	return 0;
}

int handleNeighborMessage(string data, const NeighborInfo &neighbor){
	size_t found = data.find("^", 0);
  	string typeStr;

  	if (found != string::npos) {
  		typeStr = data.substr(0, found);
  	} else {
  		return -1;
  	}

  	int type = atoi(typeStr.c_str());
  	string actualData = data.substr(found+1);

  	switch(type){
  		case CIDMessage::Advertise:
  			return handleAdvertisementMessage(actualData, neighbor);
  		case CIDMessage::Join:
  			return handleNodeJoinMessage(actualData, neighbor);
  		case CIDMessage::Leave:
  			return handleNodeLeaveMessage(actualData, neighbor);
  		default:
  			printf("Unknown message type\n");
  			return -1;
  	}
}

int processNeighborMessage(const NeighborInfo &neighbor){
	printf("receive from neighbor AD: %s HID: %s\n", neighbor.AD.c_str(), neighbor.HID.c_str());
	string data;
	int status = recvMessageFromSock(neighbor.recvSock, data);
	if(status < 0){
		printf("neighbor has closed the connection\n");
		return -1;	
	}

	if(handleNeighborMessage(data, neighbor) < 0){
		printf("handle message failed\n");
		return -2;
	}

	return 0;
}

int recvMessageFromSock(int sock, string &data){
	int n, to_recv;
	size_t remaining, size;
	char buf[IO_BUF_SIZE];

	printf("receiving CID advertisement...\n");

	n = Xrecv(sock, (char*)&remaining, sizeof(size_t), 0);
	if (n < 0) {
		printf("Xrecv failed\n");
		cleanup(0);
	}

	remaining = ntohl(remaining);
	size = remaining;

	if(remaining > XIA_MAXBUF){
		printf("received size have invalid size: %lu. Exit\n", remaining);
		cleanup(0);
	} else if(remaining == 0) {
		return -1;
	}

	while (remaining > 0) {
		to_recv = remaining > IO_BUF_SIZE ? IO_BUF_SIZE : remaining;

		n = Xrecv(sock, buf, to_recv, 0);
		if (n < 0) {
			printf("Xrecv failed\n");
			cleanup(0);
		} else if (n == 0) {
			// the other end has closed the connection for some reason
			return -1;
		}

		remaining -= n;
		string temp(buf, n);

		data += temp;
	}

	if(data.size() != size){
		printf("received data have invalid size. Should never happen\n");
		cleanup(0);
	}
	
#ifdef STATS_LOG
	logger->log("recv " + to_string(data.size()));
#endif

	return 1;
}

int sendMessageToSock(int sock, string advertisement){
	if(sock < 0){
		printf("CID advertisement send failed: sock < 0\n");
		return -1;
	}

	int sent = -1;
	size_t remaining = strlen(advertisement.c_str()), offset = 0;
	char start[remaining];
	size_t length = htonl(remaining);
	strcpy(start, advertisement.c_str());

	// first send the size of the message
	sent = Xsend(sock, (char*)&length, sizeof(size_t), 0);
	if(sent < 0){
		printf("Xsend send size failed\n");
		return -1;
	}

	// then send the actual message
	while(remaining > 0){
		sent = Xsend(sock, start + offset, remaining, 0);
		if (sent < 0) {
			printf("Xsend failed\n");
			return -1;
		}

		remaining -= sent;
		offset += sent;
	}

#ifdef STATS_LOG
	logger->log("send " + to_string(advertisement.size()));
#endif

	return 0;
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

#if defined(STATS_LOG)
   	logger = new Logger(hostname);
#endif

	// broadcast hello first
   	HelloMessage msg;
	msg.AD = routeState.myAD;
	msg.HID = routeState.myHID;
	msg.SID = routeState.mySID;
	msg.send();

	// start cid advertisement timer
	CIDAdvertiseTimer();

	// clean up resources
	(void) signal(SIGINT, cleanup);

	while(1){
		iteration++;
		if(iteration % 400 == 0){
			msg.send();
		}

		FD_ZERO(&socks);
		FD_SET(routeState.helloSock, &socks);
		FD_SET(routeState.masterSock, &socks);

		highSock = max(routeState.helloSock, routeState.masterSock);
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			if(it->second.recvSock > highSock){
				highSock = it->second.recvSock;
			}

			if(it->second.recvSock != -1){
				FD_SET(it->second.recvSock, &socks);
			}
		}

		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000;

		selectRetVal = Xselect(highSock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			if (FD_ISSET(routeState.helloSock, &socks)) {
				// if we received a hello packet from neighbor, check if it is a new neighbor
				// and then establish connection state with them
				processHelloMessage();
				// handle any neighbor that has failed silently
				checkExpiredNeighbors();
			} else if (FD_ISSET(routeState.masterSock, &socks)) {
				// if a new neighbor connects, add to the recv list
				processNeighborConnect();
			} else {
				int status;
				vector<string> candidates;

				for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
					// if we recv a message from one of our established neighbor, procees the message
					if(it->second.recvSock != -1 && FD_ISSET(it->second.recvSock, &socks)){
						status = processNeighborMessage(it->second);
						if(status == -1){
							candidates.push_back(it->first);
						}
					}
				}
				removeExpiredNeighbors(candidates);
			}
		}
	}

	return 0;
}