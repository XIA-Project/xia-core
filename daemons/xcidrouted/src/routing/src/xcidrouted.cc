#include "xcidrouted.h"

//#define LOG

using namespace std;

static char *ident = NULL;
static char* hostname = NULL;

static XIARouter xr;
static RouteState routeState;

#ifdef LOG
static Logger* logger;
#endif

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

void AdvertisementMessage::print() const{
	printf("AdvertisementMessage: \n");
	printf("\tsenderHID: %s\n", this->senderHID.c_str());
	printf("\tcurrSenderHID: %s\n", this->currSenderHID.c_str());
	printf("\tseq: %d\n", this->seq);
	printf("\tttl: %d\n", this->ttl);
	printf("\tdistance: %d\n", this->distance);

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
	if(sock < 0){
		printf("CID advertisement send failed: sock < 0\n");
		return -1;
	}

	printf("sending CID advertisement...\n");

	string advertisement = serialize();
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

	printf("sending raw CID advertisement: %s\n", start);
	printf("sending CID advertisement:\n");
	print();

#ifdef LOG
	string logStr = "send " + to_string(this->newCIDs.size() + this->delCIDs.size());
	logger->log(logStr.c_str());
#endif

	printf("sent CID advertisement\n");

	return 1;
}

int AdvertisementMessage::recv(int sock){
	int n, to_recv;
	size_t remaining, size;
	char buf[IO_BUF_SIZE];
	string data;

	printf("receiving CID advertisement...\n");

	n = Xrecv(sock, (char*)&remaining, sizeof(size_t), 0);
	if (n < 0) {
		printf("Xrecv failed\n");
		return n;
	}

	remaining = ntohl(remaining);
	size = remaining;

	while (remaining > 0) {
		to_recv = remaining > IO_BUF_SIZE ? IO_BUF_SIZE : remaining;

		n = Xrecv(sock, buf, to_recv, 0);
		if (n < 0) {
			printf("Xrecv failed\n");
			return -1;
		} else if (n == 0) {
			break;
		}

		remaining -= n;
		string temp(buf, n);

		data += temp;
	}

	if(data.size() == 0){
		return -1;
	}

	printf("received a raw advertisement message:\n");
	printf("remaining size: %lu, actual received size: %lu\n", size, data.size());
	for(int i = 0; i < (int)data.size(); i++){
		printf("%c", data[i]);
	}
	printf("\n");
	
	deserialize(data);
	print();

#ifdef LOG
	// log the number of advertised CIDs received.
	string logStr = "recv " + to_string(this->newCIDs.size() + this->delCIDs.size());
	logger->log(logStr.c_str());
#endif

	printf("received CID advertisement\n");

	return 1;
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

void cleanup(int) {
#ifdef LOG
	logger->end();
	delete logger;
#endif

	routeState.mtx.lock();
	for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
		Xclose(it->second.recvSock);
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

void CIDAdvertiseTimer(){
	thread([](){
    	// sleep for 10 seconds for hello message to propagate
		this_thread::sleep_for(chrono::seconds(INIT_WAIT_TIME_SEC));
        while (true)
        {
            double nextTimeInSecond = nextWaitTimeInSecond(CID_ADVERT_UPDATE_RATE_PER_SEC);
            int nextTimeMilisecond = (int) ceil(nextTimeInSecond * 1000);

            advertiseCIDs();

            this_thread::sleep_for(chrono::milliseconds(nextTimeMilisecond));
        }
    }).detach();
}

void advertiseCIDs(){
	printf("advertising CIDs...\n");

	AdvertisementMessage msg;
	msg.senderHID = routeState.myHID;
	msg.currSenderHID = routeState.myHID;
	msg.seq = routeState.lsaSeq;
	msg.ttl = MAX_TTL;
	msg.distance = 0;

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
			msg.delCIDs.insert(*it);
		}
	}

	// find all the new local CIDs first
	for(auto it = currLocalCIDs.begin(); it != currLocalCIDs.end(); it++){
		if(routeState.localCIDs.find(*it) == routeState.localCIDs.end()){
			msg.newCIDs.insert(*it);
		}
	}

	routeState.mtx.lock();
	routeState.localCIDs = currLocalCIDs;
	routeState.mtx.unlock();

#ifdef LOG
	//log the number of advertised CIDs received.
	string logStr = "local " + to_string(currLocalCIDs.size());
	logger->log(logStr.c_str());
#endif

	// start advertise to each of my neighbors
	if(msg.delCIDs.size() > 0 || msg.newCIDs.size() > 0){

		routeState.mtx.lock();
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			msg.send(it->second.sendSock);
		}
		routeState.mtx.unlock();

		routeState.lsaSeq = (routeState.lsaSeq + 1) % MAX_SEQNUM;
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
   	routeState.lsaSeq = 0;

   	Graph g = Node() * Node(BHID) * Node(SID_XCIDROUTE);
	g.fill_sockaddr(&routeState.ddag);
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
		routeState.mtx.unlock();
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

	string key = AD + HID;

	routeState.mtx.lock();
	int	interface = interfaceNumber("HID", HID);
	routeState.neighbors[key].recvSock = acceptSock;
	routeState.neighbors[key].AD = AD;
	routeState.neighbors[key].HID = HID;
	routeState.neighbors[key].port = interface;
	routeState.mtx.unlock();
}

void processNeighborMessage(const NeighborInfo &neighbor){
	printf("receive from neighbor AD: %s HID: %s\n", neighbor.AD.c_str(), neighbor.HID.c_str());
	// deseralize the message
	AdvertisementMessage msg;
	int status = msg.recv(neighbor.recvSock);
	if(status < 0){
		printf("message receive failed\n");
		return;
	}

	// need to check both the sequence number and ttl since message with lower
	// sequence number but higher ttl need to be propagated further
	if(routeState.HID2Seq.find(msg.senderHID) != routeState.HID2Seq.end() &&
		msg.seq < routeState.HID2Seq[msg.senderHID] && routeState.HID2Seq[msg.senderHID] - msg.seq < 1000000){	
		// we must have seen this sequence number before since all messages 
		// are sent in order
		if(routeState.HID2Seq2TTL[msg.senderHID][msg.seq] >= msg.ttl){
			return;
		} else {
			routeState.HID2Seq2TTL[msg.senderHID][msg.seq] = msg.ttl;
		}
	} else {
		routeState.HID2Seq[msg.senderHID] = msg.seq;
		routeState.HID2Seq2TTL[msg.senderHID][msg.seq] = msg.ttl;
	}

	routeState.mtx.lock();

	set<string> advertiseDeletion;
	// remove the entries that need to be removed
	for(auto it = msg.delCIDs.begin(); it != msg.delCIDs.end(); it++){
		if(routeState.CIDRoutes.find(*it) != routeState.CIDRoutes.end()){
			CIDRouteEntry currEntry = routeState.CIDRoutes[*it];

			// remove an entry only if it is from the same host
			if(currEntry.dest == msg.senderHID){
				routeState.CIDRoutes.erase(*it);
				xr.delRouteCIDRouting(*it);
			}
		}

		// only send the delete message if current router don't have local CIDs.
		// since
		// 	a) local CIDs also exists during route addition of current broadcast message:
		// 		if current router have the local CIDs, same brocast message does not
		// 		even pass through during route addition.
		// 	b) local CIDs don't exists during the route addition but added later on:
		// 		if current router have the local CIDs, it must have been broadcasted 
		// 		already AND broadcast is reliable and in-order AND it is shorter 
		// 		than CID routes from other routers through this router. So routers 
		// 		receiving the broadcast don't have routes in the deletion message.

		if(routeState.localCIDs.find(*it) == routeState.localCIDs.end()){			
			advertiseDeletion.insert(*it);
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
	routeState.mtx.unlock();

	// update the message and broadcast to other neighbor
	// 	iff there are something meaningful to broadcast.
	if(msg.ttl - 1 > 0 && (advertiseAddition.size() > 0 || msg.delCIDs.size() > 0)){
		AdvertisementMessage msg2Others;
		msg2Others.senderHID = msg.senderHID;
		msg2Others.currSenderHID = routeState.myHID;
		msg2Others.seq = msg.seq;
		msg2Others.ttl = msg.ttl - 1;
		msg2Others.distance = msg.distance + 1;
		msg2Others.newCIDs = advertiseAddition;
		msg2Others.delCIDs = advertiseDeletion;

		routeState.mtx.lock();
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			if(it->second.HID != neighbor.HID){
				msg2Others.send(it->second.sendSock);
			}
		}
		routeState.mtx.unlock();
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

#ifdef LOG
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
			} else if (FD_ISSET(routeState.masterSock, &socks)) {
				// if a new neighbor connects, add to the recv list
				processNeighborJoin();
			} else {
				for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
					// if we recv a message from one of our established neighbor, procees the message
					if(it->second.recvSock != -1 && FD_ISSET(it->second.recvSock, &socks)){
						processNeighborMessage(it->second);
					}
				}
			}
		}
	}

	return 0;
}