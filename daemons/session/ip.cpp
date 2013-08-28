/*
 * Implements IP-specific session layer helper functions.
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <ifaddrs.h>



sockaddr_in* addrFromData(const string *addr_buf) {
	return (sockaddr_in*)addr_buf->data();
}

string *bufFromAddr(sockaddr_in *sa) {
	return new string((const char*)sa, sizeof(sockaddr_in));
}

int getLocalIP(string ifname, struct in_addr *ipaddr) {

	struct ifaddrs *ifaddr, *ifa;
	int family;
	
	if (getifaddrs(&ifaddr) == -1) {
	    ERROR("getifaddrs");
		return -1;
	}
	
	/* Walk through linked list, maintaining head pointer so we
	   can free list later */
	
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
	    if (ifa->ifa_addr == NULL)
	        continue;
		
		if (string(ifa->ifa_name) == ifname) {

	    	family = ifa->ifa_addr->sa_family;
	    	if (family == AF_INET) {
				ipaddr->s_addr = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr;
				return 1;
	    	}
		}
	}
	
	freeifaddrs(ifaddr);
	return -1;
}

int _getaddrinfo(const char *name, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
	(void)service;
	(void)hints;
DBGF("getaddrinfo: %s", name);

	// resolve name
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		ERRORF("error creating socket to name server: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in sa;
	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(5353);

	session::NSMsg nsm;
	session::NSLookup *lm = nsm.mutable_lookup();
	lm->set_name(name);

	std::string p_buf;
	nsm.SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();
	if (sendto(sockfd, p, remaining, 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		ERRORF("Error sending NS lookup: %s", strerror(errno));
		return -1;
	}
		
	// receive response from name server
	char buf[1500];
	unsigned buflen = sizeof(buf);
	memset(buf, 0, buflen);

	int rc;
	socklen_t len = sizeof(sa);
	if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
		ERRORF("error(%d) receving message: %s", errno, strerror(errno));
		return -1;
	}

	nsm.ParseFromString(buf);

	if (!nsm.has_reply()) {
		ERROR("NS message did not contain reply");
		return -1;
	}


	// allocate memory needed
 	struct addrinfo *ai = (struct addrinfo *)calloc(sizeof(struct addrinfo), 1);
 	sockaddr_in *psa = (sockaddr_in *)calloc(sizeof(sockaddr_in), 1);
	if (!ai || !psa) {
		if (ai)
			free(ai);
		return EAI_MEMORY;
	}

	// fill in the blanks
	psa->sin_family = AF_INET;
	psa->sin_addr.s_addr = nsm.reply().ip();
	psa->sin_port = nsm.reply().port();

	ai->ai_family    = AF_INET;
	//ai->ai_socktype  = socktype;
	//ai->ai_protocol  = protocol;
	ai->ai_flags     = 0;
	ai->ai_addr      = (struct sockaddr *)psa;
	ai->ai_addrlen   = sizeof(sockaddr_in);
	ai->ai_next      = NULL;

	*res = ai;

	return 0;
}

int registerName(const string &name, string *addr_buf) {
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		ERRORF("error creating socket to name server: %s", strerror(errno));
		return -1;
	}

	// nameserver addr
	struct sockaddr_in sa;
	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(5353);

	// addr to register
	sockaddr_in *addr = addrFromData(addr_buf);

	session::NSMsg nsm;
	session::NSRegister *rm = nsm.mutable_reg();
	rm->set_name(name);
	rm->set_ip(addr->sin_addr.s_addr);
	rm->set_port(addr->sin_port);

	assert(rm);


	std::string p_buf;
	nsm.SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.data();
	if (sendto(sockfd, p, remaining, 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		ERRORF("Error sending NS register: %s", strerror(errno));
		return -1;
	}

    return 1;
}















string* getAddrForName(string name) {
	struct addrinfo *ai;
	sockaddr_in *sa;
	if (_getaddrinfo(name.c_str(), NULL, NULL, &ai) != 0) {
		ERRORF("unable to lookup name: %s", name.c_str());
		return NULL;
	}
	sa = (sockaddr_in*)ai->ai_addr;

	return bufFromAddr(sa);
}

string* getRemoteAddrForSocket(int sock) {
	sockaddr_in *sa = (sockaddr_in*)malloc(sizeof(sockaddr_in));
	socklen_t len = sizeof(sockaddr_in);
	if (getpeername(sock, (struct sockaddr*)sa, &len) < 0) {
		ERRORF("Error in getpeername on socket %d: %s", sock, strerror(errno));
	}
	return bufFromAddr(sa);
}

string* getLocalAddrForSocket(int sock) {
	sockaddr_in *sa = (sockaddr_in*)malloc(sizeof(sockaddr_in));
	socklen_t len = sizeof(sockaddr_in);
	if (getsockname(sock, (struct sockaddr*)sa, &len) < 0) {
		ERRORF("Error in getsockname on socket %d: %s", sock, strerror(errno));
	}
	return bufFromAddr(sa);
}

// returns an address as a string (which we use to keep track of transport
// connections going to a particular host)
string getHIDForAddr(const string *addr_buf) {
	sockaddr_in *sa = addrFromData(addr_buf);
	char *ipstr = inet_ntoa(sa->sin_addr);
	int portlen = snprintf(NULL, 0, ":%d", ntohs(sa->sin_port));
	char *portstr = (char*)malloc(portlen * sizeof(char));
	sprintf(portstr, ":%d", ntohs(sa->sin_port));

	string hid = string(ipstr) + string(portstr); // TODO: this is a hack; breaks multiplexing
	//free(ipstr);
	free(portstr);

	return hid;
}

string getHIDForName(string name) {
	// resolve name
	struct addrinfo *ai;
	sockaddr_in *sa;
	if (_getaddrinfo(name.c_str(), NULL, NULL, &ai) != 0) {
		ERRORF("unable to lookup name %s", name.c_str());
		return "ERROR";
	}
	sa = (sockaddr_in*)ai->ai_addr;

	string *addr_buf = bufFromAddr(sa);
	return getHIDForAddr(addr_buf);
}

string getHIDForSocket(int sock) {
	return getHIDForAddr(getRemoteAddrForSocket(sock));
}

int sendBuffer(session::ConnectionInfo *cinfo, const char* buf, size_t len) {
	int sock = cinfo->sockfd();
	
	//if (cinfo->type() == session::TCP) {
		return send(sock, buf, len, 0);
	//} else {
	//	ERROR("Expected a TCP socket");
	//}

	return -1; // we shouldn't get here
}

int recvBuffer(session::ConnectionInfo *cinfo, char* buf, size_t len) {
	int sock = cinfo->sockfd();

	//if (cinfo->type() == session::TCP) {
		memset(buf, 0, len);
		return recv(sock, buf, len, 0);
	//} else {
	//	ERROR("Expected a TCP socket");
	//}

	return -1; // we shouldn't get here
}

int acceptSock(int listen_sock) {
	return accept(listen_sock, NULL, NULL);
}

int closeSock(int sock) {
	return close(sock);
}

int bindRandomAddr(string **addr_buf) {

	int sfd;
	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {                
	    ERRORF("error creating socket to session process: %s", strerror(errno));
	    return -1;                                                            
	}                                                                         
	
	struct sockaddr_in sa;                      
	socklen_t len = sizeof(sa);                 
	sa.sin_family = PF_INET;                    
	//sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = 0;                            
	
	if (getLocalIP("eth0", &sa.sin_addr) < 0) {
		ERROR("Error getting local IP address");
	}
	
	if (bind(sfd, (const struct sockaddr *)&sa, len) < 0) { 
	    close(sfd);                                           
	    ERRORF("bind error: %s", strerror(errno));                     
	    return -1;                                                   
	}                                                                

	if (listen(sfd, 100) < 0) {
		ERRORF("Error calling listen() on sock: %d", sfd);
	}


	// figure out which port we bound to
	//sockaddr_in *sa = (sockaddr_in*)rp->ai_addr;
	if(getsockname(sfd, (struct sockaddr *)&sa, &len) < 0) {
	    close(sfd);
	    ERRORF("Error retrieving new socket's port: %s", strerror(errno));
	    return -1;
	}

	*addr_buf = bufFromAddr(&sa);
	return sfd;
}

int openConnectionToAddr(const string *addr_buf, session::ConnectionInfo *cinfo) {

	sockaddr_in *sa = addrFromData(addr_buf);

	// make socket
	int sock;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERROR("unable to create socket");
		return -1;
	}

	// connect
	if (connect(sock, (struct sockaddr *)sa, sizeof(sockaddr_in)) < 0) {
		ERRORF("unable to connect to the destination addr: %s", strerror(errno));
		return -1;
	}
	
	cinfo->set_sockfd(sock);
	cinfo->set_hid(getHIDForAddr(addr_buf));
	cinfo->set_addr(*addr_buf);
	cinfo->set_type(session::TCP);
	return 0;
}

int openConnectionToName(string &name, session::ConnectionInfo *cinfo) {
	
	struct addrinfo *ai;
	if (_getaddrinfo(name.c_str(), NULL, NULL, &ai) < 0) {
		ERRORF("Error looking up hostname: %s", name.c_str());
		return -1;
	}

	string *addr_buf = bufFromAddr((sockaddr_in*)ai->ai_addr);

	return openConnectionToAddr(addr_buf, cinfo);
}

// watches to see if we switch networks; makes new transport connections if so
void* mobility_daemon(void *args) {
	(void)args;

	// TODO: implement

	return NULL;
}
