/*
 * Implements XIA-specific session layer helper functions.
 */
#include "Xsocket.h"
#include "dagaddr.hpp"
#include "xssl.h"

sockaddr_x* addrFromData(const string *addr_buf) {
	return (sockaddr_x*)addr_buf->data();
}

string *bufFromAddr(sockaddr_x *sa) {
	return new string((const char*)sa, sizeof(sockaddr_x));
}

string* getAddrForName(string name) {
	struct addrinfo *ai;
	sockaddr_x *sa;
	if (Xgetaddrinfo(name.c_str(), NULL, NULL, &ai) != 0) {
		ERRORF("unable to lookup name: %s", name.c_str());
		return NULL;
	}
	sa = (sockaddr_x*)ai->ai_addr;

	return bufFromAddr(sa);
}

string* getRemoteAddrForSocket(int sock) {
	sockaddr_x *sa = (sockaddr_x*)malloc(sizeof(sockaddr_x));
	socklen_t len = sizeof(sockaddr_x);
	if (Xgetpeername(sock, (struct sockaddr*)sa, &len) < 0) {
		ERRORF("Error in Xgetpeername on socket %d: %s", sock, strerror(errno));
	}
	return bufFromAddr(sa);
}

string* getLocalAddrForSocket(int sock) {
	sockaddr_x *sa = (sockaddr_x*)malloc(sizeof(sockaddr_x));
	socklen_t len = sizeof(sockaddr_x);
	if (Xgetsockname(sock, (struct sockaddr*)sa, &len) < 0) {
		ERRORF("Error in Xgetsockname on socket %d: %s", sock, strerror(errno));
	}
	return bufFromAddr(sa);
}

// returns an address's HID as a string (which we use to keep track of transport
// connections going to a particular host)
string getHIDForAddr(const string *addr_buf) {
	sockaddr_x *sa = addrFromData(addr_buf);
	Graph g(sa);
	vector<const Node*> hids = g.get_nodes_of_type(Node::XID_TYPE_HID);

	if (hids.size() > 0) {
		return hids[0]->to_string();
	} else {
		ERROR("DAG contained no HIDs");
		return "ERROR";
	}
}

string getHIDForName(string name) {
	// resolve name
	struct addrinfo *ai;
	sockaddr_x *sa;
	if (Xgetaddrinfo(name.c_str(), NULL, NULL, &ai) != 0) {
		ERRORF("unable to lookup name %s", name.c_str());
		return "ERROR";
	}
	sa = (sockaddr_x*)ai->ai_addr;

	string *addr_buf = bufFromAddr(sa);
	return getHIDForAddr(addr_buf);
}

string getHIDForSocket(int sock) {
	return getHIDForAddr(getRemoteAddrForSocket(sock));
}

int sendBuffer(session::ConnectionInfo *cinfo, const char* buf, size_t len) {

	if (cinfo->transport_protocol() == session::XSP) {
		return Xsend(cinfo->sockfd(), buf, len, 0);
	}

	return -1; // we shouldn't get here
}

int recvBuffer(session::ConnectionInfo *cinfo, char* buf, size_t len) {

	if (cinfo->transport_protocol() == session::XSP) {
		memset(buf, 0, len);
		return Xrecv(cinfo->sockfd(), buf, len, 0);
	}

	return -1; // we shouldn't get here
}

int acceptSock(int listen_sock) {
	return Xaccept(listen_sock, NULL, 0);
}

int closeSock(int sock) {
	return Xclose(sock);
}


int bindWithKeypair(RSA *keypair, string **addr_buf) {

	char *SID = SID_from_keypair(keypair);
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID, NULL, &ai) != 0) {
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

int bindNewSocket(session::ConnectionInfo *cinfo, string **addr_buf) {
	if (!cinfo->has_ssl_ctx_ptr()) {
		XSSL_CTX *xssl_ctx = XSSL_CTX_new();
		if (xssl_ctx == NULL) {
			ERROR("Unable to init new XSSL context");
			return -1;
		}
		cinfo->set_ssl_ctx_ptr(&xssl_ctx, sizeof(XSSL_CTX*));
	}
	return bindWithKeypair((*(XSSL_CTX**)cinfo->ssl_ctx_ptr().data())->keypair, addr_buf);
}

int bindNewSocket(session::ContextInfo *ctxInfo, string **addr_buf) {
	if (!ctxInfo->has_ssl_ctx_ptr()) {
		XSSL_CTX *xssl_ctx = XSSL_CTX_new();
		if (xssl_ctx == NULL) {
			ERROR("Unable to init new XSSL context");
			return -1;
		}
		ctxInfo->set_ssl_ctx_ptr(&xssl_ctx, sizeof(XSSL_CTX*));
	}
	return bindWithKeypair((*(XSSL_CTX**)ctxInfo->ssl_ctx_ptr().data())->keypair, addr_buf);
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
DBGF("    opened connection, sock is %d", sock);

	cinfo->set_sockfd(sock);
	cinfo->set_hid(getHIDForAddr(addr_buf));
	cinfo->set_addr(*addr_buf);

	return 0;

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
			if (migrate_connections() < 0) {
				ERROR("Error migrating connections");
			}
			my_addr = g;
		}

		sleep(MOBILITY_CHECK_INTERVAL);
	}


	return NULL;
}

