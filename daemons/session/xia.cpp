/*
 * Implements XIA-specific session layer helper functions.
 */
#include "Xsocket.h"
#include "dagaddr.hpp"

sockaddr_x* addrFromData(const string *addr_buf) {
	return (sockaddr_x*)addr_buf->data();
}

int sendBuffer(session::ConnectionInfo *cinfo, const char* buf, size_t len) {
	int sock = cinfo->sockfd();
	
	if (cinfo->type() == session::XSP) {
		return Xsend(sock, buf, len, 0);
	}

	return -1; // we shouldn't get here
}

int recvBuffer(session::ConnectionInfo *cinfo, char* buf, size_t len) {
	int sock = cinfo->sockfd();

	if (cinfo->type() == session::XSP) {
		memset(buf, 0, len);
		return Xrecv(sock, buf, len, 0);
	}

	return -1; // we shouldn't get here
}

int acceptSock(int listen_sock) {
	return Xaccept(listen_sock, NULL, 0);
}

int closeSock(int sock) {
	return Xclose(sock);
}

int bindRandomAddr(string **addr_buf) {
	// make DAG to bind to (w/ random SID)
	unsigned char buf[20];
	uint32_t i;
	srand(time(NULL));
	for (i = 0; i < sizeof(buf); i++) {
	    buf[i] = rand() % 255 + 1;
	}
	char sid[45];
	sprintf(&(sid[0]), "SID:");
	for (i = 0; i < 20; i++) {
		sprintf(&(sid[i*2 + 4]), "%02x", buf[i]);
	}
	sid[44] = '\0';
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, sid, NULL, &ai) != 0) {
		ERROR("getaddrinfo failure!\n");
		return -1;
	}
	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	// make socket and bind
	int sock;
	if ((sock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0) {
		ERROR("unable to create the listen socket");
		return -1;
	}
	if (Xbind(sock, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0) {
		ERROR("unable to bind");
		return -1;
	}

	 *addr_buf = new string((const char*)sa, sizeof(sockaddr_x));
	return sock;
}


int openConnectionToAddr(const string *addr_buf, session::ConnectionInfo *cinfo) {

	sockaddr_x *sa = addrFromData(addr_buf);
	
	// make socket
	int sock;
	if ((sock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0) {
		ERROR("unable to create the server socket");
		return -1;
	}

	// connect
	if (Xconnect(sock, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0) {
		ERROR("unable to connect to the destination dag");
		return -1;
	}
LOGF("    opened connection, sock is %d", sock);

	cinfo->set_sockfd(sock);
	return 1;

}

int openConnectionToName(const string &name, session::ConnectionInfo *cinfo) {

	// resolve name
	struct addrinfo *ai;
	sockaddr_x *sa;
	if (Xgetaddrinfo(name.c_str(), NULL, NULL, &ai) != 0) {
		ERRORF("unable to lookup name %s", name.c_str());
		return -1;
	}
	sa = (sockaddr_x*)ai->ai_addr;

	string *addr_buf = new string((const char*)sa, sizeof(sockaddr_x));
	return openConnectionToAddr(addr_buf, cinfo);
}

int registerName(const string &name, string *addr_buf) {
    return XregisterName(name.c_str(), addrFromData(addr_buf));
}



// watches to see if we switch networks; makes new transport connections if so
void* mobility_daemon(void *args) {
	(void)args;
	struct addrinfo *ai;
	sockaddr_x *sa;
		
	// Get starting DAG
	if (Xgetaddrinfo(NULL, NULL, NULL, &ai) != 0) {
		ERROR("getaddrinfo failure!\n");
	}
	sa = (sockaddr_x*)ai->ai_addr;
	Graph my_addr(sa);



	// Now watch for changes
	while (true) {
		if (Xgetaddrinfo(NULL, NULL, NULL, &ai) != 0) {
			ERROR("getaddrinfo failure!\n");
			continue;
		}
		sa = (sockaddr_x*)ai->ai_addr;
		Graph g(sa);

		if ( g.dag_string() != my_addr.dag_string() ) { // TODO: better comparison?
			LOG("We moved!");
			if (migrate_connections() < 0) {
				ERROR("Error migrating connections");
			}
			my_addr = g;
		}

		sleep(2);
	}


	return NULL;
}
