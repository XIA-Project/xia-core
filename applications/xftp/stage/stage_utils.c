#include "stage_utils.h"

int verbose = 1;

void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

void warn(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}

void die(int ecode, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "Exiting\n");
	exit(ecode);
}

//split the str     --Lwy   1.16
vector<string> strVector(char *strs)
{
	char str_arr[strlen(strs)];
	strcpy(str_arr, strs);
	vector<string> strVec;
	char *saveptr;
	char *str = strtok_r(str_arr, " ", &saveptr);
	strVec.push_back(str);
	while ((str = strtok_r(NULL, " ", &saveptr)) != NULL) {
		strVec.push_back(str);
	}

	return strVec;
}
//change the return type into const char*   --Lwy   1.16
const char *getStageServiceName()
{
	return string2char(string(STAGE_SERVER_NAME) + "." + getAD());
}

const char *getStageManagerName()
{
	return string2char(string(STAGE_MANAGER_NAME) + "." + getHID());
}

const char *getXftpName()
{
	return string2char(string(FTP_NAME));
}
//TODO:consider how to free the memory  --Lwy   1.16
char* string2char(string str)
{
	char *cstr = new char[str.length() + 1];
	strcpy(cstr, str.c_str());
	return cstr;
}

long string2long(string str)
{
	//stringstream buffer(str);
	long var;
	//buffer >> var;
	sscanf(str.c_str(),"%ld",&var);
	return var;
}

string execSystem(string cmd)
{
	FILE* pipe = popen(string2char(cmd), "r");
	if (!pipe) return NULL;
    char buffer[128];
    string result = "";
    while (!feof(pipe)) {
		if (fgets(buffer, 128, pipe) != NULL)
			result += buffer;
	}
	pclose(pipe);

	if (result.empty()) return result;
	result.erase(result.end()-1, result.end()); // remove the newline character
	return result;
}

bool file_exists(const char * filename)
{
	if (FILE * file = fopen(filename, "r")) {
		fclose(file);
		return true;
	}
	return false;
}

int getRTT(const char * host){
	char cmd[128] = "";
	sprintf(cmd,"./xping -qi 0.1 %s 56 5 | grep \"/[0-9]*/\" -o ", host);
	string ans = execSystem(cmd);
	int result;
	sscanf(ans.c_str(),"/%d/",&result);
	return result;
}

long now_msec()
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) == 0)
		return ((tv.tv_sec * 1000000L + tv.tv_usec) / 1000);
	else
		return -1;
}
long long now_usec()
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) == 0)
		return (tv.tv_sec * 1000000L + tv.tv_usec);
	else
		return -1;
}
string getSSID()
{
	string ssid = execSystem(GETSSID_CMD);

	if (ssid.empty()) {
// cerr<<"No network\n";
		while (1) {
			usleep(SCAN_DELAY_MSEC * 1000);
			ssid = execSystem(GETSSID_CMD);
			if (!ssid.empty()) {
// cerr<<"Network back\n";
				break;
			}
		}
	}
	return ssid;
}
bool isConnect()
{
	string ssid = execSystem(GETSSID_CMD);
	return !ssid.empty();
}
string getAD()
{
	int sock;

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	char ad[MAX_XID_SIZE], hid[MAX_XID_SIZE], ip[MAX_XID_SIZE];

	if (XreadLocalHostAddr(sock, ad, sizeof(ad), hid, sizeof(hid), ip, sizeof(ip)) < 0)
		die(-1, "Reading localhost address\n");

	Xclose(sock);

	return string(ad);
}

string getHID()
{
	int sock;

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	char ad[MAX_XID_SIZE], hid[MAX_XID_SIZE], ip[MAX_XID_SIZE];

	if (XreadLocalHostAddr(sock, ad, sizeof(ad), hid, sizeof(hid), ip, sizeof(ip)) < 0)
		die(-1, "Reading localhost address\n");

	Xclose(sock);

	return string(hid);
}

void getNewAD(char *old_ad)
{
	int sock;

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");
	char new_ad[MAX_XID_SIZE], hid[MAX_XID_SIZE], ip[MAX_XID_SIZE];

	while (1) {
		if (XreadLocalHostAddr(sock, new_ad, sizeof(new_ad), hid, sizeof(hid), ip, sizeof(ip)) < 0)
			die(-1, "Reading localhost address\n");
		if (strcmp(new_ad, old_ad) != 0) {
cerr<<"AD changed!"<<endl;
			strcpy(old_ad, new_ad);
			Xclose(sock);
			return;
		}
		usleep(SCAN_DELAY_MSEC * 1000);
	}
	return;
}

string netConnStatus(string lastSSID)
{
	string currSSID = execSystem(GETSSID_CMD);

	if (currSSID.empty())	{
		return "empty";
	}
	else {
		if (currSSID == lastSSID) {
			return "same";
		}
		else {
			return currSSID;
		}
	}
}
/*  Send $cmd to $sa with $sock and store the respose to $reply
 *
 *  return value:
 *      -2  :   No Network
 *      -3  :   Timeout
 *      otherwise   size of reply btye
 *
 *      --Lwy   1.16
 */
int getReply(int sock, const char *cmd, char *reply, sockaddr_x *sa, int timeout, int tries)
{
	int sent, received, rc;

	for (int i = 0; i < tries; i++) {
		string currSSID = execSystem(GETSSID_CMD);
		if (currSSID.empty())
			return -2;
		else {
			if ((sent = Xsendto(sock, cmd, strlen(cmd), 0, (sockaddr*)sa, sizeof(sockaddr_x))) < 0) {
				die(-4, "Send error %d on socket %d\n", errno, sock);
			}

			say("Xsock %4d sent %d bytes\n", sock, sent);

			struct pollfd pfds[2];
			pfds[0].fd = sock;
			pfds[0].events = POLLIN;
			if ((rc = Xpoll(pfds, 1, timeout)) <= 0) {
				say("Will poll next time\n");
				//die(-5, "Poll returned %d\n", rc);
			}

			memset(reply, 0, strlen(reply));
            //ERROR?    strlen(reply) is wrong??    --Lwy   1.16
			//if ((received = Xrecvfrom(sock, reply, strlen(reply), 0, NULL, NULL)) < 0)
			if ((received = Xrecvfrom(sock, reply, sizeof(reply), 0, NULL, NULL)) < 0)
				die(-5, "Receive error %d on socket %d\n", errno, sock);
			else {
				say("Xsock %4d received %d bytes\n", sock, received);
				return received;
			}
		}
	}
	return -3;
}

int sendStreamCmd(int sock, const char *cmd)
{
	warn("Sending Command: %s \n", cmd);
	int n;
	if ((n = Xsend(sock, cmd,  strlen(cmd), 0)) < 0) {
		Xclose(sock);
		die(-1, "Unable to communicate\n");
	}
	return n;
}
/* No Use for sayHello, so I change it into Marco
 * and #define sayHello sendStreamCmd in header
 *          --Lwy   1.16
int sayHello(int sock, const char *helloMsg)
{
	int m = sendStreamCmd(sock, helloMsg);
	return m;
}
*/

int hearHello(int sock)
{
	//say("HearHello Start!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n\n\n\n\n\n");
	char command[XIA_MAXBUF];
	memset(command, '\0', strlen(command));
	int n;
	if ((n = Xrecv(sock, command, XIA_MAXBUF, 0))  < 0) {
		warn("socket error while waiting for data, closing connection\n");
	}
	//say("HearHello printf!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1\n\n\n\n\n\n\n\n");
	printf("%s\n", command);
	return n;
	/*
	if (strncmp(command, "get", 3) == 0) {
		sscanf(command, "get %s %d %d", fin, &start, &end);
		printf("get %s %d %d\n", fin, start, end);
	if (strncmp(command, "Hello from context client", 25) == 0) {
		say("Received hello from context client\n");
		char* hello = "Hello from prefetch client";
		sendCmd(prefetchProfileSock, hello);
	}
	*/
}

int XgetRemoteAddr(int sock, char *ad, char *hid, char *sid)
{
	sockaddr_x dag;
	socklen_t daglen = sizeof(dag);
	char sdag[1024];

	if (Xgetpeername(sock, (struct sockaddr*)&dag, &daglen) < 0)
		die(-1, "unable to locate: %s\n", sock);

	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
	// say("sdag = %s\n",sdag);
	char *ads = strstr(sdag, "AD:");	// first occurrence
	char *hids = strstr(sdag, "HID:");
	char *sids = strstr(sdag, "SID:");
	if (sscanf(ads, "%s", ad) < 1 || strncmp(ad, "AD:", 3) != 0) {
		die(-1, "Unable to extract AD.");
	}
	if (sscanf(hids, "%s", hid) < 1 || strncmp(hid, "HID:", 4) != 0) {
		die(-1, "Unable to extract HID.");
	}
	if (sscanf(sids, "%s", sid) < 1 || strncmp(sid, "SID:", 4) != 0) {
		die(-1, "Unable to extract SID.");
	}
	return 1;
}

int XgetServADHID(const char *name, char *ad, char *hid)
{
	sockaddr_x dag;
	socklen_t daglen = sizeof(dag);
	char sdag[1024];
	if (XgetDAGbyName(name, &dag, &daglen) < 0)
		die(-1, "unable to locate: %s\n", name);
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
	// say("sdag = %s\n",sdag);
	char *ads = strstr(sdag, "AD:");	// first occurrence
	char *hids = strstr(sdag, "HID:");

	if (sscanf(ads, "%s", ad) < 1 || strncmp(ad, "AD:", 3) != 0) {
		die(-1, "Unable to extract AD.");
	}
	if (sscanf(hids, "%s", hid) < 1 || strncmp(hid, "HID:", 4) != 0) {
		die(-1, "Unable to extract HID.");
	}

	return 1;
}

int tDatagramClient(const char *name, struct addrinfo *ai, sockaddr_x *sa)
{
	if (Xgetaddrinfo(name, NULL, NULL, &ai) != 0)
		die(-1, "unable to lookup name %s\n", name);
	sa = (sockaddr_x*)ai->ai_addr;

	Graph g(sa);
	printf("\n%s\n", g.dag_string().c_str());

	int sock;
	if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0) {
		die(-2, "unable to create the socket\n");
	}
	say("Xsock %4d created\n", sock);

	return sock;
}

int reXgetDAGbyName(const char *name, sockaddr_x *addr, socklen_t *addrlen)
{
	int result;
	for (int i = 0; i < NS_LOOKUP_RETRY_NUM; i++) {
		printf("-----------Retried %d/%d times...\n", i + 1, NS_LOOKUP_RETRY_NUM);
		if ((result = XgetDAGbyName(name, addr, addrlen)) >= 0) {
			break;
		}
	}
	return result;
}

int initStreamClient(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid)
{
	int sock, rc;
	sockaddr_x dag;
	socklen_t daglen;
	char sdag[1024];
	char IP[MAX_XID_SIZE];

	// lookup the xia service
	daglen = sizeof(dag);
cerr<<"Before got DAG by name: "<<name<<"\n";
	if (reXgetDAGbyName(name, &dag, &daglen) < 0)
		die(-1, "unable to locate: %s\n", name);
cerr<<"Got DAG by name\n";
	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");
cerr<<"Created socket\n";
	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}
cerr<<"Connected to peer\n";
	rc = XreadLocalHostAddr(sock, src_ad, MAX_XID_SIZE, src_hid, MAX_XID_SIZE, IP, MAX_XID_SIZE);

	if (rc < 0) {
		Xclose(sock);
		die(-1, "Unable to read local address.\n");
	}
	else{
		warn("My AD: %s, My HID: %s\n", src_ad, src_hid);
	}

	// save the AD and HID for later. This seems hacky we need to find a better way to deal with this
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
	// say("sdag = %s\n",sdag);
	char *ads = strstr(sdag, "AD:");
	char *hids = strstr(sdag, "HID:");

	if (sscanf(ads, "%s", dst_ad) < 1 || strncmp(dst_ad, "AD:", 3) != 0) {
		die(-1, "Unable to extract AD.");
	}
	if (sscanf(hids, "%s", dst_hid) < 1 || strncmp(dst_hid, "HID:", 4) != 0) {
		die(-1, "Unable to extract HID.");
	}
	warn("Service AD: %s, Service HID: %s\n", dst_ad, dst_hid);
	return sock;
}

int registerDatagramReceiver(char* name)
{
	int sock;

	if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
		die(-2, "unable to create the datagram socket\n");

	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	// Generate an SID to use
	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	Graph g((sockaddr_x*)ai->ai_addr);
	printf("\nDatagram DAG\n%s\n", g.dag_string().c_str());

	if (XregisterName(name, sa) < 0)
		die(-1, "error registering name: %s\n", name);

	say("registered name: \n%s\n", name);

	if (Xbind(sock, (sockaddr *)sa, sizeof(sa)) < 0) {
		die(-3, "unable to bind to the dag\n");
	}
	return sock;
}

int registerStreamReceiver(const char* name, char *myAD, char *myHID, char *my4ID)
{
	int sock;

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	// read the localhost AD and HID
	if (XreadLocalHostAddr(sock, myAD, sizeof(myAD), myHID, sizeof(myHID), my4ID, sizeof(my4ID)) < 0)
		die(-1, "Reading localhost address\n");

	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	// Generate an SID to use
	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}

	struct addrinfo hints, *ai;
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;

	if (Xgetaddrinfo(NULL, sid_string, &hints, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	Graph g((sockaddr_x*)ai->ai_addr);

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	if (Xbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to bind to the dag: %s\n", g.dag_string().c_str());
	}
	say("listening on dag: %s\n", g.dag_string().c_str());

	Xlisten(sock, 5);

	if (XregisterName(name, sa) < 0 ) {
		die(-1, "error registering name: %s\n", name);
	}
	say("\nRegistering DAG with nameserver:\n%s\n", g.dag_string().c_str());

  return sock;
}

void *blockListener(void *listenID, void *recvFuntion (void *))
{
  int listenSock = *((int*)listenID);
  //int acceptSock;

  //change acceptSock with dynamically allocated in case of race    --Lwy   1.16
  //TODO:BUT HOW TO FREE IT ????
  while (1) {
		say("Waiting for a client connection\n");

		//if ((acceptSock = Xaccept(listenSock, NULL, NULL)) < 0){
    int *acceptSock = new int;
		if ((*acceptSock = Xaccept(listenSock, NULL, NULL)) < 0){
			die(-1, "accept failed\n");
		}
		say("connected\n");

		pthread_t client;
		//pthread_create(&client, NULL, recvFuntion, (void *)&acceptSock);
		pthread_create(&client, NULL, recvFuntion, (void *)acceptSock);
	}

	Xclose(listenSock);

	return NULL;
}

void *twoFunctionBlockListener(void *listenID, void *oneRecvFuntion (void *), void *twoRecvFuntion (void *))
{
  int listenSock = *((int*)listenID);
  //int acceptSock;

  //change acceptSock with dynamically allocated in case of race    --Lwy   1.16
  //TODO:BUT HOW TO FREE IT ????
  while (1) {
		say("Waiting for a client connection\n");

       int *acceptSock = (int*)malloc(sizeof(int));
		//if ((acceptSock = Xaccept(listenSock, NULL, NULL)) < 0){
		if ((*acceptSock = Xaccept(listenSock, NULL, NULL)) < 0){
			die(-1, "accept failed\n");
		}
		say("connected\n");

		pthread_t client, fetchData;
		//pthread_create(&client, NULL, oneRecvFuntion, (void *)&acceptSock);
		//pthread_create(&fetchData, NULL, twoRecvFuntion, (void *)&acceptSock);
		pthread_create(&client, NULL, oneRecvFuntion, (void *)acceptSock);
		pthread_create(&fetchData, NULL, twoRecvFuntion, (void *)acceptSock);
	}

	Xclose(listenSock);

	return NULL;
}

int getIndex(string target, vector<string> pool) {
	for (unsigned i = 0; i < pool.size(); i++) {
		if (target == pool[i])
			return i;
	}
	return -1;
}

int registerStageService(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid)
{
	int sock, rc;
	sockaddr_x dag;
	socklen_t daglen;
	char sdag[1024];
	char IP[MAX_XID_SIZE];

cerr << "--------------------------1" << endl;
say("------------------------name is: %s\n", name);
say("------------------------myAD is: %s\n", src_ad);
say("------------------------myHID is: %s\n", src_hid);
say("------------------------stageAD is: %s\n", dst_ad);
say("------------------------stageHID is: %s\n", dst_hid);
	// lookup the xia service
	daglen = sizeof(dag);
cerr << "--------------------daglen is: " << daglen << endl;
	if (XgetDAGbyName(name, &dag, &daglen) < 0) {
cerr << "-----------------------name is: " << name << endl;
		die(-1, "unable to locate: %s\n", name);
	}
cerr << "-------------------------2" << endl;
	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		die(-1, "Unable to create the listening socket\n");
	}
say("------------------------------3\n");
	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}
say("------------------------------4\n");
	rc = XreadLocalHostAddr(sock, src_ad, MAX_XID_SIZE, src_hid, MAX_XID_SIZE, IP, MAX_XID_SIZE);

	if (rc < 0) {
		Xclose(sock);
		die(-1, "Unable to read local address.\n");
	}
	else{
		warn("My AD: %s, My HID: %s\n", src_ad, src_hid);
	}
say("----------------------------------5\n");

	// save the AD and HID for later. This seems hacky we need to find a better way to deal with this
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
	// say("sdag = %s\n",sdag);
	char *ads = strstr(sdag, "AD:");
	char *hids = strstr(sdag, "HID:");

	if (sscanf(ads, "%s", dst_ad) < 1 || strncmp(dst_ad, "AD:", 3) != 0) {
		die(-1, "Unable to extract AD.");
	}
	if (sscanf(hids, "%s", dst_hid) < 1 || strncmp(dst_hid, "HID:", 4) != 0) {
		die(-1, "Unable to extract HID.");
	}
	warn("Service AD: %s, Service HID: %s\n", dst_ad, dst_hid);
say("-------------------------------------6\n");
	return sock;
}

int registerStageManager(const char *name)
{
	int sock;
	sockaddr_x dag;
	socklen_t daglen;

	// lookup the xia service
	daglen = sizeof(dag);

	if (XgetDAGbyName(name, &dag, &daglen) < 0) {
		warn("unable to locate: %s\n", name);
	}
	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		warn("Unable to create the listening socket\n");
	}
	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		warn("Unable to bind to the dag: %s\n", dag);
		sock = -1;
	}
	return sock;
}

int updateManifest(int sock, vector<string> CIDs)
{
say("In updateManifest()\n");
	char cmd[XIA_MAX_BUF];
	int offset = 0;
	int count = CIDs.size();
	int num;
	while (offset < count) {
		num = MAX_CID_NUM;
		if (count - offset < MAX_CID_NUM) {
			num = count - offset;
		}
		memset(cmd, '\0', strlen(cmd));
		sprintf(cmd, "reg cont");
		for (int i = offset; i < offset + num; i++) {
			strcat(cmd, " ");
			strcat(cmd, string2char(CIDs[i]));
		}
		offset += MAX_CID_NUM;
		send(sock, cmd, strlen(cmd),0);
		usleep(SCAN_DELAY_MSEC*1000);
	}
	memset(cmd, '\0', strlen(cmd));
	sprintf(cmd, "reg done");
	//if (Xsend(sock, cmd, strlen(cmd), 0) < 0) {
	if (send(sock, cmd, strlen(cmd), 0) < 0) {
		warn("unable to send reply to client\n");
	}
	say("Waiting for manager finish profile.\n");
	say("In updateManifest, register to manager done.");
	usleep(SCAN_DELAY_MSEC*1000);

	return 0;
}

// TODO: add fallback
int XrequestChunkStage(int sock, const ChunkStatus *cs) {
	char cmd[XIA_MAX_BUF];
	memset(cmd, '\0', strlen(cmd));
	sprintf(cmd, "fetch %ld %s\n", cs[0].cidLen, cs[0].cid);

/*
	char AD[MAX_XID_SIZE], HID[MAX_XID_SIZE], IP[MAX_XID_SIZE];

	if (XreadLocalHostAddr(sock, AD, sizeof(AD), HID, sizeof(HID), IP, sizeof(IP)) < 0)
		die(-1, "Reading localhost address\n");

	char *CID = chunkReqDag2cid(cs[0].cid);

	// rewrite the AD and HID of the chunk request
	char *dag = (char *)malloc(512);
	sprintf(dag, "RE ( %s %s ) CID:%s", AD, HID, CID);

	sprintf(cmd, "fetch %ld %s", strlen(dag), dag);
*/
	//int n = sendStreamCmd(sock, cmd);
	int n = send(sock,cmd,strlen(cmd),0);
	//hearHello(sock);
	return n;
}

/*
// TODO: right now it's hacky, need to fix the way reading XIDs when including fallback DAG
char *chunkReqDag2cid(char *dag) {
	char *cid = (char *)malloc(512);
	char *cids = strstr(dag, "CID:");
	if (sscanf(cids, "%s", cid) < 1 || strncmp(cid, "CID:", 4) != 0) {
		die(-1, "Unable to extract AD.");
	}
cerr<<"CID: "<<cid+4<<endl;
	return cid+4;
}

char *getPrefetchManagerName()
{
	return string2char(string(PREFETCH_MANAGER_NAME) + "." + getHID());
}

char *getPrefetchServiceName()
{
	return string2char(string(PREFETCH_SERVER_NAME) + "." + getAD());
}

int registerPrefetchManager(const char *name)
{
	int sock;
	sockaddr_x dag;
	socklen_t daglen;

	// lookup the xia service
	daglen = sizeof(dag);

	if (XgetDAGbyName(name, &dag, &daglen) < 0) {
		warn("unable to locate: %s\n", name);
	}
	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		warn("Unable to create the listening socket\n");
	}
	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		warn("Unable to bind to the dag: %s\n", dag);
		sock = -1;
	}
	return sock;
}

int registerPrefetchService(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid)
{
	int sock, rc;
	sockaddr_x dag;
	socklen_t daglen;
	char sdag[1024];
	char IP[MAX_XID_SIZE];

	// lookup the xia service
	daglen = sizeof(dag);
	if (XgetDAGbyName(name, &dag, &daglen) < 0)
		die(-1, "unable to locate: %s\n", name);
	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");
	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}
	rc = XreadLocalHostAddr(sock, src_ad, MAX_XID_SIZE, src_hid, MAX_XID_SIZE, IP, MAX_XID_SIZE);

	if (rc < 0) {
		Xclose(sock);
		die(-1, "Unable to read local address.\n");
	}
	else{
		warn("My AD: %s, My HID: %s\n", src_ad, src_hid);
	}

	// save the AD and HID for later. This seems hacky we need to find a better way to deal with this
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
	// say("sdag = %s\n",sdag);
	char *ads = strstr(sdag, "AD:");
	char *hids = strstr(sdag, "HID:");

	if (sscanf(ads, "%s", dst_ad) < 1 || strncmp(dst_ad, "AD:", 3) != 0) {
		die(-1, "Unable to extract AD.");
	}
	if (sscanf(hids, "%s", dst_hid) < 1 || strncmp(dst_hid, "HID:", 4) != 0) {
		die(-1, "Unable to extract HID.");
	}
	warn("Service AD: %s, Service HID: %s\n", dst_ad, dst_hid);

	return sock;
}

int updateManifestOld(int sock, vector<string> CIDs)
{
	char cmd[XIA_MAX_BUF];
	memset(cmd, '\0', strlen(cmd));
	char cids[XIA_MAX_BUF];
	memset(cids, '\0', strlen(cids));

	for (unsigned int i = 0; i < CIDs.size(); i++) {
		strcat(cids, " ");
		strcat(cids, string2char(CIDs[i]));
	}
	// TODO: check total length of cids should not exceed max buf
	sprintf(cmd, "reg%s", cids);
	int n = sendStreamCmd(sock, cmd);

	return n;
}
*/
//add unix style socket	--Lwy	1.20
int registerUnixStreamReceiver(const char *servername){
	int fd;
  struct sockaddr_un un;
  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)  {
     return(-1);
  }
  int len, rval;
  unlink(servername);               /* in case it already exists */
  memset(&un, 0, sizeof(un));
  un.sun_family = AF_UNIX;
  strcpy(un.sun_path, servername);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(servername);
  /* bind the name to the descriptor */
  if (bind(fd, (struct sockaddr *)&un, len) < 0){
    rval = -2;
  }
  else{
      if (listen(fd, 5) < 0)      {
        rval =  -3;
      }
      else {
        return fd;
      }
  }
  int err;
  err = errno;
  close(fd);
  errno = err;
  return rval;
}
int UnixBlockListener(void* listenID, void* recvFuntion (void*)){
	int listenSock = *(int*)listenID;
	struct sockaddr_un un;
	socklen_t len = sizeof(un);
	while (1) {
		say("Waiting for a Unix client connection\n");

		//if ((acceptSock = Xaccept(listenSock, NULL, NULL)) < 0){
		int *acceptSock = new int;
		if ((*acceptSock = accept(listenSock, (struct sockaddr *)&un, &len)) < 0){
			die(-1, "Unix accept failed\n");
		}
		say("connected\n");

		pthread_t client;
		//pthread_create(&client, NULL, recvFuntion, (void *)&acceptSock);
		pthread_create(&client, NULL, recvFuntion, (void *)acceptSock);
	}

	close(listenSock);

	return 0;
}
int registerUnixStageManager(const char* servername){
	int fd;
  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)    /* create a UNIX domain stream socket */
  {
    return(-1);
  }
  int len, rval;
  struct sockaddr_un un;
  memset(&un, 0, sizeof(un));            /* fill socket address structure with our address */
  un.sun_family = AF_UNIX;
  sprintf(un.sun_path, "/tmp/scktmp%05d", getpid());
  len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
  unlink(un.sun_path);               /* in case it already exists */
  if (bind(fd, (struct sockaddr *)&un, len) < 0)
  {
     rval=  -2;
  }
  else
  {
    /* fill socket address structure with server's address */
      memset(&un, 0, sizeof(un));
      un.sun_family = AF_UNIX;
      strcpy(un.sun_path, servername);
      len = offsetof(struct sockaddr_un, sun_path) + strlen(servername);
      if (connect(fd, (struct sockaddr *)&un, len) < 0)
      {
          rval= -4;
      }
      else
      {
         return (fd);
      }
  }
  int err;
  err = errno;
  close(fd);
  errno = err;
  return rval;
}
