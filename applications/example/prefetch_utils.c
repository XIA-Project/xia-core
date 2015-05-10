#include "prefetch_utils.h"

int verbose = 1;

void say(const char *fmt, ...) {
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

void warn(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}

void die(int ecode, const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "Exiting\n");
	exit(ecode);
}

char** str_split(char* a_str, const char *a_delim) {
	char** result = 0;
	int count = 0;
	int str_len = strlen(a_str);
	int del_len = strlen(a_delim);
	int i = 0;
	int j = 0;
	char* last_delim = 0;

	/* Count how many elements will be extracted. */
	for (i = 0; i < str_len; i++) 
		for (j = 0; j < del_len; j++) 
			if(a_str[i] == a_delim[j]) {
				count++;
				last_delim = &a_str[i];
			}
	 /* Add space for trailing token. */
	count += last_delim < (a_str + strlen(a_str) - 1);
	
 	/* Add space for terminating null string so caller
			knows where the list of returned strings ends. */
 	count++;

	result = (char **)malloc(sizeof(char*) * count);
	
	// printf ("Splitting string \"%s\" into %i tokens:\n", a_str, count);
	
	i = 0;
	result[i] = strtok(a_str, a_delim);
	// printf ("%s\n",result[i]);
	
	for(i = 1; i < count; i++) {
		result[i] = strtok (NULL, a_delim);
		// printf ("%s\n",result[i]);
	}

	return result;
}

void usage() {
	say("usage: get|put <source file> <dest name>\n");
}

bool file_exists(const char * filename) {
	if (FILE * file = fopen(filename, "r")) {
		fclose(file);
		return true;
	}
	return false;
}

int sendCmd(int sock, const char *cmd) {

	warn("Sending Command: %s \n", cmd);
	int n;
	if ((n = Xsend(sock, cmd,  strlen(cmd), 0)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate\n");
	}
	return n;
}

int sayHello(int sock, const char *helloMsg) {
	int m = sendCmd(sock, helloMsg); 
	return m;
}

int hearHello(int sock) { //, const char *helloMsg) {
	char command[XIA_MAXBUF];
	memset(command, '\0', strlen(command));
	int n;
	if ((n = Xrecv(sock, command, RECV_BUF_SIZE, 0))  < 0) {
		warn("socket error while waiting for data, closing connection\n");
	}
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

int initializeClient(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid) {
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
	} else{
		warn("My AD: %s, My HID: %s\n", src_ad, src_hid);
	}
	
	// save the AD and HID for later. This seems hacky
	// we need to find a better way to deal with this
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
	// say("sdag = %s\n",sdag);
	char *ads = strstr(sdag,"AD:");
	char *hids = strstr(sdag,"HID:");
	
	if (sscanf(ads, "%s", dst_ad) < 1 || strncmp(dst_ad, "AD:", 3) != 0) {
		die(-1, "Unable to extract AD.");
	}
		
	if (sscanf(hids, "%s", dst_hid) < 1 || strncmp(dst_hid, "HID:", 4) != 0) {
		die(-1, "Unable to extract HID.");
	}

	warn("Service AD: %s, Service HID: %s\n", dst_ad, dst_hid);
	return sock;
}

int getChunkCount(int sock, char *reply, int sz) {
	int n = -1;

	if ((n = Xrecv(sock, reply, sz, 0))  < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}

	if (strncmp(reply, "OK:", 3) != 0) {
		warn( "%s\n", reply);
		return -1;
	}

	reply[n] = 0;

	return n;
}

int buildChunkDAGs(ChunkStatus cs[], char *chunks, char *dst_ad, char *dst_hid) {
	char *p = chunks;
	char *next;
	int n = 0;

	char *dag;
	
	// build the list of chunks to retrieve
	while ((next = strchr(p, ' '))) {
		*next = 0;
		dag = (char *)malloc(512);
		sprintf(dag, "RE ( %s %s ) CID:%s", dst_ad, dst_hid, p);
		// printf("built dag: %s\n", dag);
		cs[n].cidLen = strlen(dag);
		cs[n].cid = dag;
		n++;
		p = next + 1;
	}
	dag = (char *)malloc(512);
	sprintf(dag, "RE ( %s %s ) CID:%s", dst_ad, dst_hid, p);
	// printf("getting %s\n", p);
	cs[n].cidLen = strlen(dag);
	cs[n].cid = dag;
	n++;
	return n;
}

int getListedChunks(int csock, FILE *fd, char *chunks, char *dst_ad, char *dst_hid) {
	ChunkStatus cs[NUM_CHUNKS];
	char data[XIA_MAXCHUNK];
	int len;
	int status;
	int n = -1;
	
	n = buildChunkDAGs(cs, chunks, dst_ad, dst_hid);
	
	// NOTE: the chunk transport is not currently reliable and chunks may need to be re-requested
	// ask for the current chunk list again every REREQUEST seconds
	// chunks already in the local cache will not be refetched from the network 
	// read the the whole chunk list first before fetching
	unsigned ctr = 0;
	while (1) {
		if (ctr % REREQUEST == 0) {
			// bring the list of chunks local
			say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
			if (XrequestChunks(csock, cs, n) < 0) {
				say("unable to request chunks\n");
				return -1;
			}
			say("checking chunk status\n");
		}
		ctr++;

		status = XgetChunkStatuses(csock, cs, n);

		if (status == READY_TO_READ)
			break;

		else if (status < 0) {
			say("error getting chunk status\n");
			return -1;
		} 
		else if (status & WAITING_FOR_CHUNK) {
			say("waiting... one or more chunks aren't ready yet\n");
		} 
		else if (status & INVALID_HASH) {
			die(-1, "one or more chunks has an invalid hash");
		} 
		else if (status & REQUEST_FAILED) {
			die(-1, "no chunks found\n");
		} 
		else {
			say("unexpected result\n");
		}
		sleep(1);
	}

	say("all chunks ready\n");

	for (int i = 0; i < n; i++) {
		char *cid = strrchr(cs[i].cid, ':');
		cid++;
		say("reading chunk %s\n", cid);
		if ((len = XreadChunk(csock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
			say("error getting chunk\n");
			return -1;
		}
		// say("writing %d bytes of chunk %s to disk\n", len, cid);
		fwrite(data, 1, len, fd);

		free(cs[i].cid);
		cs[i].cid = NULL;
		cs[i].cidLen = 0;
	}
	return n;
}

// register the service with the name server and open the necessary sockets
int registerStreamReceiver(char* name, char *myAD, char *myHID, char *my4ID) {
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

	struct addrinfo *ai;

	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;
	// FIXME: NAME is hard coded
  if (XregisterName(name, dag) < 0)
		die(-1, "error registering name: %s\n", name);

	if (Xbind(sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	Graph g(dag);
	say("listening on dag: %s\n", g.dag_string().c_str());
  return sock;  
}

void *blockListener(void *listenID, void *recvFuntion (void *)) {
  int listenSock = *((int*)listenID);
  int acceptSock;

  while (1) {
		say("Waiting for a client connection\n");
   		
		if ((acceptSock = Xaccept(listenSock, NULL, NULL)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");
		
		// handle the connection in a new thread
		pthread_t client;
		pthread_create(&client, NULL, recvFuntion, (void *)&acceptSock);
	}
	
	Xclose(listenSock); // we should never reach here!
	return NULL;
}