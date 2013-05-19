#include "session.pb.h"
#include "utils.hpp"
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <map>
#include <queue>
#include <stdio.h>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#define DEFAULT_PROCPORT 1989
#define BUFSIZE 65000

#define DEBUG
#define XIA

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif

using namespace std;

/* DATA STRUCTURES */
map<unsigned short, session::SessionInfo*> ctx_to_session_info; 
map<unsigned short, session::ConnectionInfo*> ctx_to_txconn;
map<unsigned short, session::ConnectionInfo*> ctx_to_rxconn;
map<unsigned short, int> ctx_to_listensock;
map<string, session::ConnectionInfo*> name_to_conn;
map<uint32_t, unsigned short> incomingctx_to_myctx; // key: upper 16 = sender's ctx, lower 16 = incoming sockfd

map<int, pthread_t*> sockfd_to_pthread;  // could be a listen sock or data sock (not both)

map<string, unsigned short> name_to_listen_ctx;
map<unsigned short, queue<session::SessionPacket*>* > listen_ctx_to_acceptQ;
map<unsigned short, pthread_mutex_t*> ctx_to_acceptQ_mutex;
map<unsigned short, pthread_cond_t*> ctx_to_acceptQ_cond;

map<unsigned short, queue<session::SessionPacket*>* > ctx_to_dataQ;
map<unsigned short, pthread_mutex_t*> ctx_to_dataQ_mutex;
map<unsigned short, pthread_cond_t*> ctx_to_dataQ_cond;

string sockconf_name = "sessiond";
int procport = DEFAULT_PROCPORT;

struct proc_func_data {
	int (*process_function)(int, session::SessionMsg&, session::SessionMsg&);
	int sockfd;
	struct sockaddr_in *sa;
	socklen_t len;
	int ctx;
	session::SessionMsg *msg;
	session::SessionMsg *reply;
};


/* FORWARD DECLARATIONS */
void *poll_recv_sock(void *args);




/* PRINT FUNCTIONS */
void print_sessions() {
	printf("*******************************************************************************\n");
	printf("*                                 SESSIONS                                    *\n");
	printf("*******************************************************************************\n\n");
	
	map<unsigned short, session::SessionInfo*>::iterator iter;
	for (iter = ctx_to_session_info.begin(); iter!= ctx_to_session_info.end(); ++iter) {
		session::SessionInfo* sinfo = iter->second;

		printf("Context:\t%d\n", iter->first);	
		printf("Forward path:\t");
		for (int i = 0; i < sinfo->forward_path_size(); i++) {
			printf("%s  ", sinfo->forward_path(i).name().c_str());
		}
		printf("\nReturn path:\t");
		for (int i = 0; i < sinfo->return_path_size(); i++) {
			printf("%s  ", sinfo->return_path(i).name().c_str());
		}

		if (sinfo->has_my_name()) {
			printf("\nMy name:\t%s", sinfo->my_name().c_str());
		}
		printf("\n\n-------------------------------------------------------------------------------\n\n");
	}
}

void print_connections() {
	printf("*******************************************************************************\n");
	printf("*                               CONNECTIONS                                   *\n");
	printf("*******************************************************************************\n\n");
	
	map<string, session::ConnectionInfo*>::iterator iter;
	for (iter = name_to_conn.begin(); iter!= name_to_conn.end(); ++iter) {
		session::ConnectionInfo *cinfo = iter->second;

		printf("Name:\t\t%s\n", iter->first.c_str());
		printf("Sockfd:\t\t%d\n", cinfo->sockfd());
		printf("Sessions:\t");
		for (int i = 0; i < cinfo->sessions_size(); i++) {
			printf("%d  ", cinfo->sessions(i));
		}
		printf("\nInterface:\t%s", cinfo->interface().c_str());
		//printf("Type:\t%s", cinfo->type());

		printf("\n\n-------------------------------------------------------------------------------\n\n");
	}
}



/* HELPER FUNCTIONS */

/*
** display cmd line options and exit
*/
void help()
{
	printf("\nsessiond -- Session Layer Daemon\n");
	printf("usage: sessiond [-np] \n");
	printf("where:\n");
	printf(" -n : sockconf name \n");
	printf(" -p : listen port \n");
	printf("\n");
	exit(0);
}

/*
** configure the app
*/
void getConfig(int argc, char** argv)
{
	int c;
	while ((c = getopt(argc, argv, "hn:p:")) != -1) {
		switch (c) {
			case 'n':
				sockconf_name = string(optarg);
				break;
			case 'p':
				procport = atoi(optarg);
				break;
			case '?':
			case 'h':
			default:
				// Help Me!
				help();
		}
	}
}

void allocate_dataQ(int ctx) {
	// allocate a data Q
	queue<session::SessionPacket*> *dataQ = new queue<session::SessionPacket*>();
	ctx_to_dataQ[ctx] = dataQ;

	// allocate new pthread vars for this session's data Q
	pthread_mutex_t *mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutex, NULL);
	ctx_to_dataQ_mutex[ctx] = mutex;
	pthread_cond_t *cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	pthread_cond_init(cond, NULL);
	ctx_to_dataQ_cond[ctx] = cond;
}

#ifdef XIA
int bind_random_addr_xia(sockaddr_x **sa) {
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
		LOG("getaddrinfo failure!\n");
		return -1;
	}
	*sa = (sockaddr_x*)ai->ai_addr;

	// make socket and bind
	int sock;
	if ((sock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0) {
		LOG("unable to create the listen socket");
		return -1;
	}
	if (Xbind(sock, (struct sockaddr *)*sa, sizeof(sockaddr_x)) < 0) {
		LOG("unable to bind");
		return -1;
	}
	return sock;
}
#endif /* XIA */

void infocpy(const session::SessionInfo *from, session::SessionInfo *to) {
	for (int i = 0; i < from->forward_path_size(); i++) {
		session::SessionInfo_ServiceInfo *serviceInfo = to->add_forward_path();
		serviceInfo->set_name(from->forward_path(i).name());
	}
	for (int i = 0; i < from->return_path_size(); i++) {
		session::SessionInfo_ServiceInfo *serviceInfo = to->add_return_path();
		serviceInfo->set_name(from->return_path(i).name());
	}

	if (from->has_my_name()) {
		to->set_my_name(from->my_name());
	}

	if (from->has_my_addr()) {
		to->set_my_addr(from->my_addr());
	}

	if (from->has_initiator_addr()) {
		to->set_initiator_addr(from->initiator_addr());
	}
}

// name is last hop if it is second to last in the return path or if it is the
// last name in the forward path and the return path contains only the initiator
bool is_last_hop(int ctx, string name) {
	session::SessionInfo *info = ctx_to_session_info[ctx];
	
	// Look for name in the return path
	int found_index = -1;
	for (int i = 0; i < info->return_path_size(); i++) {
		if (info->return_path(i).name() == name) {
			found_index = i;
			break;
		}
	}

	if (found_index != -1 && found_index == info->return_path_size() - 2) {
		return true;
	} else if (info->return_path_size() == 1 && 
				info->forward_path(info->forward_path_size()-1).name() == name) {
		return true;
	}
	
	return false;
}

string get_neighborhop_name(string my_name, const session::SessionInfo *info, bool next) {
	// Look for my_name in the forward path
	int found_index = -1;
	for (int i = 0; i < info->forward_path_size(); i++) {
		if (info->forward_path(i).name() == my_name) {
			found_index = i;
			break;
		}
	}

	// If we found it, the next hop is either the next entry in the forward path
	// or the first entry in the return path
	if ( found_index != -1 ) {
		if (next) {  // finding next hop
			if ( found_index == info->forward_path_size()-1 ) {
				return info->return_path(0).name();
			} else {
				return info->forward_path(found_index+1).name();
			}
		} else {  // finding previous hop
			if ( found_index == 0) {
				return info->return_path(info->return_path_size()-1).name();
			} else {
				return info->forward_path(found_index-1).name();
			}
		}
	}
	// If we didn't find it, look for my_name in the return path
	for (int i = 0; i < info->return_path_size(); i++) {
		if (info->return_path(i).name() == my_name) {
			found_index = i;
			break;
		}
	}

	// The next hop is either the next entry in the return path or the first entry
	// in the forward path
	if ( found_index != -1 ) {
		if (next) {  // finding next hop
			if ( found_index == info->return_path_size()-1 ) {
				return info->forward_path(0).name();
			} else {
				return info->return_path(found_index+1).name();
			}
		} else {  // finding previous hop
			if ( found_index == 0 ) {
				return info->forward_path(info->forward_path_size()-1).name();
			} else {
				return info->return_path(found_index-1).name();
			}
		}
	}

	return "NOT FOUND";
}

string get_neighborhop_name(int ctx, bool next) {
	session::SessionInfo *info = ctx_to_session_info[ctx];
	string my_name = info->my_name();
	return get_neighborhop_name(my_name, info, next);
}

string get_nexthop_name(int ctx) {
	return get_neighborhop_name(ctx, true);
}

string get_prevhop_name(int ctx) {
	return get_neighborhop_name(ctx, false);
}

#ifdef XIA
int open_connection_xia(sockaddr_x *sa, session::ConnectionInfo *cinfo) {
	
	// make socket
	int sock;
	if ((sock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0) {
		LOG("unable to create the server socket");
		return -1;
	}

	// connect
	if (Xconnect(sock, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0) {
		LOG("unable to connect to the destination dag");
		return -1;
	}
LOGF("    opened connection, sock is %d", sock);

	cinfo->set_sockfd(sock);
	return 1;

}
#endif /* XIA */


int open_connection(string name, session::ConnectionInfo *cinfo) {
#ifdef XIA
	// resolve name
	struct addrinfo *ai;
	sockaddr_x *sa;
	if (Xgetaddrinfo(name.c_str(), NULL, NULL, &ai) != 0) {
		LOGF("unable to lookup name %s", name.c_str());
		return -1;
	}
	sa = (sockaddr_x*)ai->ai_addr;

	int ret = open_connection_xia(sa, cinfo);
#endif /* XIA */
	
	if (ret >= 0) {
		name_to_conn[name] = cinfo;

		// start a thread reading this socket
		pthread_t *t = (pthread_t*)malloc(sizeof(pthread_t));
		int pthread_rc;
		if ( (pthread_rc = pthread_create(t, NULL, poll_recv_sock, (void*)cinfo)) < 0) {
			LOGF("Error creating recv sock thread: %s", strerror(pthread_rc));
			return -1;
		}
		sockfd_to_pthread[cinfo->sockfd()] = t;
	
	}

	return ret;
}

int get_txconn_for_context(int ctx, session::ConnectionInfo **cinfo) {
	if ( ctx_to_txconn.find(ctx) != ctx_to_txconn.end() ) {
		// This session already has a transport conn
		*cinfo = ctx_to_txconn[ctx];
		return (*cinfo)->sockfd();
	} else {
		// There isn't a tx transport conn for this session yet 
		
		// Get the next-hop hostname
		string nexthop = get_nexthop_name(ctx);

		// If no one else has opened a connection to this name, open one
		if ( name_to_conn.find(nexthop) == name_to_conn.end() ) {
			*cinfo = new session::ConnectionInfo();
			if ( open_connection(nexthop, *cinfo) < 0 ) {
				return -1;
			}
		} else {
			*cinfo = name_to_conn[nexthop];
		}

			
		// now add this context to the connection so it doesn't get "garbage collected"
		// if all the other sessions using it close
		(*cinfo)->add_sessions(ctx);  // TODO: remove when session closes

		ctx_to_txconn[ctx] = *cinfo;

		// TODO: check that the connection is of the correct type (TCP vs UDP etc)

		return (*cinfo)->sockfd();
	}
}

int get_rxconn_for_context(int ctx, session::ConnectionInfo **cinfo) {
	if ( ctx_to_rxconn.find(ctx) != ctx_to_rxconn.end() ) {
		// This session already has a transport conn
		*cinfo = ctx_to_rxconn[ctx];
		return (*cinfo)->sockfd();
	} else {
		// There isn't a rx transport conn for this session yet 
		
		// Get the prev-hop hostname
		string prevhop = get_prevhop_name(ctx);

		// If no one else has opened a connection to this name, open one
		if ( name_to_conn.find(prevhop) == name_to_conn.end() ) {
			*cinfo = new session::ConnectionInfo();
			if ( open_connection(prevhop, *cinfo) < 0 ) {
				return -1;
			}
		} else {
			*cinfo = name_to_conn[prevhop];
		}

			
		// now add this context to the connection so it doesn't get "garbage collected"
		// if all the other sessions using it close
		(*cinfo)->add_sessions(ctx);  // TODO: remove when session closes

		ctx_to_rxconn[ctx] = *cinfo;

		// TODO: check that the connection is of the correct type (TCP vs UDP etc)

		return (*cinfo)->sockfd();
	}
}

// TODO: deal with fragmentation
// sends to arbitrary transport connection
int send(session::ConnectionInfo *cinfo, session::SessionPacket *pkt) {
	int sock = cinfo->sockfd();
	int sent = -1;

	string p_buf;
	pkt->SerializeToString(&p_buf);
	int len = p_buf.size();
	const char *buf = p_buf.c_str();

	if (cinfo->type() == session::XSP) {
#ifdef XIA
		if ((sent = Xsend(sock, buf, len, 0)) < 0) {
			LOGF("Send error %d on socket %d, dest name %s: %s", errno, sock, cinfo->name().c_str(), strerror(errno));
		}
#endif /* XIA */
	} else {
	}

	return sent;
}

// sends to next hop
int send(int ctx, session::SessionPacket *pkt) {
	session::ConnectionInfo *cinfo = NULL;
	get_txconn_for_context(ctx, &cinfo);
	return send(cinfo, pkt);
}

int recv(session::ConnectionInfo *cinfo, session::SessionPacket *pkt) {
	int sock = cinfo->sockfd();
	int received = -1;
	char buf[BUFSIZE];


	if (cinfo->type() == session::XSP) {
#ifdef XIA
		memset(buf, 0, BUFSIZE);
		if ((received = Xrecv(sock, buf, BUFSIZE, 0)) < 0) {
			LOGF("Receive error %d on socket %d: %s", errno, sock, strerror(errno));
		} else {
			string *bufstr = new string(buf, received);
			pkt->ParseFromString(*bufstr);
		}
#endif /* XIA */
	} else {
	}

	return received;
}

/*int recv(int ctx, session::SessionPacket *pkt) {
	session::ConnectionInfo *cinfo = NULL;
	get_rxconn_for_context(ctx, &cinfo);
	return recv(cinfo, pkt);
}*/




/* POLLING FUNCTIONS */

// TODO: Good way to report failures from these functions?
void * poll_listen_sock(void * args) {
LOG("BEGIN poll_listen_sock");

	int ctx = (int)args;   // TOOD: check this
	int listen_sock = ctx_to_listensock[ctx];

	while(true)
	{
		int new_rxsock = -1;
		
		// accept connection on listen sock
	#ifdef XIA
		if ( (new_rxsock = Xaccept(listen_sock, NULL, 0)) < 0 ) {
			LOGF("Error accepting new connection on listen context %d", ctx);
			//rcm->set_message("Error accpeting new connection on listen context");
			//rcm->set_rc(session::FAILURE);
			//return -1;
		}
	#endif /* XIA */
	LOGF("    Accepted new connection on sockfd %d", ctx_to_listensock[ctx]);
	
	
		session::ConnectionInfo *rx_cinfo = new session::ConnectionInfo();
		rx_cinfo->set_sockfd(new_rxsock);
	
		
		// receive first message (using new_ctx, not ctx) and
		// make sure it's a session info or lasthop handshake msg
		session::SessionPacket *rpkt = new session::SessionPacket();
		if ( recv(rx_cinfo, rpkt) < 0 ) {
			LOGF("Error receiving on listen sock for context %d", ctx);
			//rcm->set_message("Error receiving on new connection");
			//rcm->set_rc(session::FAILURE);
			//return -1;
		}
		if (rpkt->type() != session::SESSION_INFO || !rpkt->has_info()) {
			LOGF("Received a packet on the listen sock for contex %d that was not a SESSION_INFO msg.", ctx);
			//rcm->set_message("First packet was not a SESSION_INFO msg.");
			//rcm->set_rc(session::FAILURE);
			//return -1;
		}
	LOGF("    Received SESSION_INFO packet on listen sock for context %d", ctx);
	
	
		// Get the prevhop's name so we can store this transport connection
		string prevhop = rpkt->info().my_name();
		name_to_conn[prevhop] = rx_cinfo;
	
	
		// Add session info pkt to acceptQ
		pthread_mutex_lock(ctx_to_acceptQ_mutex[ctx]);
		listen_ctx_to_acceptQ[ctx]->push(rpkt);
		pthread_mutex_unlock(ctx_to_acceptQ_mutex[ctx]);
		pthread_cond_signal(ctx_to_acceptQ_cond[ctx]);

LOGF("    Added session info packet to accept Q %p", listen_ctx_to_acceptQ[ctx]);
LOGF("    There are %d packets in the Q", listen_ctx_to_acceptQ[ctx]->size());

		// start a thread reading this socket
		pthread_t *t = (pthread_t*)malloc(sizeof(pthread_t));
		int pthread_rc;
		if ( (pthread_rc = pthread_create(t, NULL, poll_recv_sock, (void*)rx_cinfo)) < 0) {
			LOGF("Error creating recv sock thread: %s", strerror(pthread_rc));
		}
		sockfd_to_pthread[rx_cinfo->sockfd()] = t;

	}

	return NULL;
}

// TODO: take in how much data should be read and buffer unwanted data somewhere
void *poll_recv_sock(void *args) {

	session::ConnectionInfo *cinfo = (session::ConnectionInfo*)args;
	int sock = cinfo->sockfd();

	while(true)
	{
		session::SessionPacket *rpkt = new session::SessionPacket();
		if ( recv(cinfo, rpkt) < 0 ) {
			LOGF("Error receiving on data sock %d", sock);
			//rcm->set_message("Error receiving on new connection");
			//rcm->set_rc(session::FAILURE);
			//return -1;
		}

		// process packet according to its type
		switch(rpkt->type())
		{
			case session::SESSION_INFO:
			{
				// Add session info pkt to acceptQ
				string my_name = get_neighborhop_name(rpkt->info().my_name(), &(rpkt->info()), true);
				int listen_ctx = name_to_listen_ctx[my_name];
				pthread_mutex_lock(ctx_to_acceptQ_mutex[listen_ctx]);
				listen_ctx_to_acceptQ[listen_ctx]->push(rpkt);
				pthread_mutex_unlock(ctx_to_acceptQ_mutex[listen_ctx]);
				pthread_cond_signal(ctx_to_acceptQ_cond[listen_ctx]);
				break;
			}
			case session::DATA:
			{
				// get the context this packet belongs to
				uint32_t key = rpkt->sender_ctx() << 8;
				key += sock;
				int ctx = incomingctx_to_myctx[key];

				// Add data pkt to dataQ
				pthread_mutex_lock(ctx_to_dataQ_mutex[ctx]);
				ctx_to_dataQ[ctx]->push(rpkt);
				pthread_mutex_unlock(ctx_to_dataQ_mutex[ctx]);
				pthread_cond_signal(ctx_to_dataQ_cond[ctx]);
				break;
			}
			default:
				LOGF("Received a packet of unknown type on socket %d", sock);
		}
	}

	return NULL;
}










/* MESSAGE PROCESSING  FUNCTIONS */

int process_new_context_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_new_context_msg");

	// allocate an empty session state protobuf object for this session
	session::SessionInfo* info = new session::SessionInfo();
	ctx_to_session_info[ctx] = info;

	allocate_dataQ(ctx);

	// return success
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	rcm->set_rc(session::SUCCESS);

	return 0;
}

int process_init_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_init_msg");
	session::SInitMsg im = msg.s_init();
LOGF("    Initiating a session from %s", im.my_name().c_str());
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();

	// retrieve this session's state
	session::SessionInfo *info = ctx_to_session_info[ctx];

	// parse paths into session state msg
	vector<string> forwardNames = split(im.forward_path(), ',');
	vector<string> returnNames = split(im.return_path(), ',');
	for (vector<string>::iterator it = forwardNames.begin(); it != forwardNames.end(); ++it) {
		session::SessionInfo_ServiceInfo *serviceInfo = info->add_forward_path();
		serviceInfo->set_name(trim(*it));
	}
	for (vector<string>::iterator it = returnNames.begin(); it != returnNames.end(); ++it) {
		session::SessionInfo_ServiceInfo *serviceInfo = info->add_return_path();
		serviceInfo->set_name(trim(*it));
	}

	// set my name, and if I'm not the last hop in the return path, add me
	info->set_my_name(trim(im.my_name()));
	if (info->return_path(info->return_path_size()-1).name() != info->my_name()) {
		session::SessionInfo_ServiceInfo *serviceInfo = info->add_return_path();
		serviceInfo->set_name(info->my_name());
	}


	// If necessary, make a socket for the last hop to connect to
	string prevhop = get_prevhop_name(ctx);
	int fake_listen_ctx = -1;
	int lsock = -1; // might not get used
	if ( name_to_conn.find(prevhop) == name_to_conn.end() ) {  // NOT ALREADY A CONNECTION
		// make a socket to accept connection from last hop
#ifdef XIA
		sockaddr_x *sa = NULL;
		lsock = bind_random_addr_xia(&sa);
		if (lsock < 0) {
			LOG("Error binding to random SID");
			rcm->set_message("Error binding to random SID");
			rcm->set_rc(session::FAILURE);
			return -1;
		}

	
		// store addr in session info
		string *strbuf = new string((const char*)sa, sizeof(sockaddr_x));
		info->set_my_addr(*strbuf);
		info->set_initiator_addr(*strbuf);
#endif /* XIA */
LOG("    Made socket to accept synack from last hop");
	
	} else {
		// make a "fake" listen context and accept Q for this name
		// if there isn't one already
		if (name_to_listen_ctx.find(info->my_name()) == name_to_listen_ctx.end()) {
			fake_listen_ctx = ctx;
			name_to_listen_ctx[info->my_name()] = ctx;

			queue<session::SessionPacket*> *acceptQ = new queue<session::SessionPacket*>();
			listen_ctx_to_acceptQ[fake_listen_ctx] = acceptQ;
			pthread_mutex_t *mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
			pthread_mutex_init(mutex, NULL);
			ctx_to_acceptQ_mutex[ctx] = mutex;
			pthread_cond_t *cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
			pthread_cond_init(cond, NULL);
			ctx_to_acceptQ_cond[ctx] = cond;
		} else {
			fake_listen_ctx = name_to_listen_ctx[info->my_name()];
		}
	}

	// send session info to next hop
	session::SessionPacket pkt;
	pkt.set_type(session::SESSION_INFO); 
	pkt.set_sender_ctx(ctx);
	session::SessionInfo *newinfo = pkt.mutable_info();// TODO: better way?
	infocpy(info, newinfo);
	if (send(ctx, &pkt) < 0) {
		LOG("Error sending session info to next hop");
		rcm->set_message("Error sending session info to next hop");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
LOG("    Sent connection request to next hop");




	session::ConnectionInfo *cinfo = NULL;
	session::SessionPacket *rpkt = new session::SessionPacket();
	int rxsock = -1;
	if ( name_to_conn.find(prevhop) == name_to_conn.end() ) {  // NOT ALREADY A CONNECTION
		// accept connection on listen sock
#ifdef XIA
		if ( (rxsock = Xaccept(lsock, NULL, 0)) < 0 ) {
			LOGF("Error accepting connection from last hop on context %d", ctx);
			rcm->set_message("Error accepting connection from last hop");
			rcm->set_rc(session::FAILURE);
			return -1;
		}
		Xclose(lsock);
#endif /* XIA */
LOGF("    Accepted a new connection on rxsock %d", rxsock);

		// store this rx transport conn
		cinfo = new session::ConnectionInfo();
		cinfo->set_name(prevhop);
		cinfo->set_sockfd(rxsock);
		name_to_conn[prevhop] = cinfo;
		// listen for "synack" -- receive and check incoming session info msg
		if ( recv(cinfo, rpkt) < 0) {
			LOG("Error receiving synack session info msg");
			rcm->set_message("Error receiving synack session info msg");
			rcm->set_rc(session::FAILURE);
			return -1;
		} 
	} else {
		cinfo = name_to_conn[prevhop];
		rxsock = cinfo->sockfd();
		// get synack session info from accept Q
		queue<session::SessionPacket*> *acceptQ = listen_ctx_to_acceptQ[fake_listen_ctx];
		pthread_mutex_t *mutex = ctx_to_acceptQ_mutex[fake_listen_ctx];
		pthread_cond_t *cond = ctx_to_acceptQ_cond[fake_listen_ctx];
		pthread_mutex_lock(mutex);
		if (acceptQ->size() ==0) pthread_cond_wait(cond, mutex);  // wait until there's a message
		rpkt = acceptQ->front();
		acceptQ->pop();
		pthread_mutex_unlock(mutex);
		// TODO: free acceptQ? maybe only if fake_listen_ctx == ctx?
	}

	cinfo->add_sessions(ctx);
	ctx_to_rxconn[ctx] = cinfo;
	
	
	
	// TODO: check that this is the correct session info pkt
	if (rpkt->type() != session::SESSION_INFO) {
		LOG("Expecting final synack, but packet was not a SESSION_INFO msg.");
		rcm->set_message("Expecting final synack, but packet was not a SESSION_INFO msg.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	string sender_name = rpkt->info().my_name();
LOGF("    Got final synack from: %s", sender_name.c_str());
	
	// store incoming ctx mapping
	uint32_t key = rpkt->sender_ctx() << 8;
	key += rxsock;
	incomingctx_to_myctx[key] = ctx;

	// start a thread reading this socket
	pthread_t *t = (pthread_t*)malloc(sizeof(pthread_t));
	int pthread_rc;
	if ( (pthread_rc = pthread_create(t, NULL, poll_recv_sock, (void*)cinfo)) < 0) {
		LOGF("Error creating recv sock thread: %s", strerror(pthread_rc));
		return -1;
	}
	sockfd_to_pthread[cinfo->sockfd()] = t;
	
	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_bind_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_bind_msg");
	const session::SBindMsg bm = msg.s_bind();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	// store this "listen context's" session info
	session::SessionInfo *sinfo = new session::SessionInfo();
	sinfo->set_my_name(bm.name());
LOGF("    Binding to name: %s", bm.name().c_str());

#ifdef XIA
	// bind to random SID
	sockaddr_x *sa = NULL;
	int sock = bind_random_addr_xia(&sa);
	if (sock < 0) {
		LOG("Error binding to random SID");
		rcm->set_message("Error binding to random SID");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
LOG("    Bound to random XIA SID:");
Graph g(sa);
g.print_graph();
	
	// store addr in session info
	string *strbuf = new string((const char*)sa, sizeof(sockaddr_x));
	sinfo->set_my_addr(*strbuf);


	// store port as this context's listen sock. This context does not have
	// tx or rx sockets
	ctx_to_listensock[ctx] = sock;

LOGF("    Registering name: %s", bm.name().c_str());
	// register name
    if (XregisterName(bm.name().c_str(), sa) < 0 ) {
    	LOGF("error registering name: %s\n", bm.name().c_str());
		rcm->set_message("Error registering name");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
LOG("    Registered name");
#endif /* XIA */

	// actually store the mapping if everything worked
	ctx_to_session_info[ctx] = sinfo;
	
	// store a mapping of name -> listen_ctx
	name_to_listen_ctx[bm.name()] = ctx;

	// initialize a queue to store incoming Connection Requests
	// (these might arrive on the accept socket or on another socket,
	// since the session layer shares transport connections among sessions)
	queue<session::SessionPacket*> *acceptQ = new queue<session::SessionPacket*>();
	listen_ctx_to_acceptQ[ctx] = acceptQ;

	// kick off a thread draining the listen socket into the acceptQ
	pthread_mutex_t *mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutex, NULL);
	ctx_to_acceptQ_mutex[ctx] = mutex;
	pthread_cond_t *cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	pthread_cond_init(cond, NULL);
	ctx_to_acceptQ_cond[ctx] = cond;

	pthread_t *t = (pthread_t*)malloc(sizeof(pthread_t));
	int pthread_rc;
	if ( (pthread_rc = pthread_create(t, NULL, poll_listen_sock, (void*)ctx)) < 0) {
		LOGF("Error creating listen sock thread: %s", strerror(pthread_rc));
		rcm->set_message("Error creating listen sock thread");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	sockfd_to_pthread[sock] = t;
	
	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_accept_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_accept_msg");
	
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	const session::SAcceptMsg am = msg.s_accept();
	uint32_t new_ctx = am.new_ctx();
	allocate_dataQ(new_ctx);
	
	session::SessionInfo *listen_info = ctx_to_session_info[ctx];

	// pull message from acceptQ
	queue<session::SessionPacket*> *acceptQ = listen_ctx_to_acceptQ[ctx];
	pthread_mutex_t *mutex = ctx_to_acceptQ_mutex[ctx];
	pthread_cond_t *cond = ctx_to_acceptQ_cond[ctx];
	pthread_mutex_lock(mutex);
	if (acceptQ->size() ==0) pthread_cond_wait(cond, mutex);  // wait until there's a message
	session::SessionPacket *rpkt = acceptQ->front();
	acceptQ->pop();
	pthread_mutex_unlock(mutex);


	// store a copy of the session info, updating my_name and my_addr
	session::SessionInfo *sinfo = new session::SessionInfo(rpkt->info());
	sinfo->set_my_name(listen_info->my_name());
	sinfo->clear_my_addr();
	ctx_to_session_info[new_ctx] = sinfo;
	
	// save  rx connection by name for others to use
	string prevhop = get_prevhop_name(new_ctx);
LOGF("    Got conn req from %s on context %d", prevhop.c_str(), new_ctx);
	session::ConnectionInfo *rx_cinfo = name_to_conn[prevhop];
	rx_cinfo->add_sessions(new_ctx);
	ctx_to_rxconn[new_ctx] = rx_cinfo; // TODO: set this in api call

	
	// store incoming ctx mapping
	uint32_t key = rpkt->sender_ctx() << 8;
	key += ctx_to_rxconn[new_ctx]->sockfd();
	incomingctx_to_myctx[key] = new_ctx;


	// connect to next hop
	session::SessionPacket pkt;
	pkt.set_type(session::SESSION_INFO); 
	pkt.set_sender_ctx(new_ctx);
	session::SessionInfo *newinfo = pkt.mutable_info();// TODO: better way?
	infocpy(sinfo, newinfo);
	
	// If i'm the last hop, the initiator doesn't have a name in the name service.
	// So, we need to use the DAG that was supplied by the initiator and build the
	// tx_conn ourselves. (Unless we already have a connection open with them.)
	if ( is_last_hop(new_ctx, sinfo->my_name()) ) {
LOG("    I'm the last hop; connecting to initiator with supplied addr");
		
		string nexthop = get_nexthop_name(new_ctx);
		session::ConnectionInfo *tx_cinfo = NULL;
		if ( name_to_conn.find(nexthop) == name_to_conn.end() ) { // NO CONNECTION ALREADY
			tx_cinfo = new session::ConnectionInfo();
			tx_cinfo->set_name(nexthop);
		
#ifdef XIA
			if (!sinfo->has_initiator_addr()) {
				LOG("Initiator did not send address");
				rcm->set_message("Initiator did not send address");
				rcm->set_rc(session::FAILURE);
				return -1;
			}

			const char* addr_buf = sinfo->initiator_addr().data();
			sockaddr_x *sa = (sockaddr_x*)addr_buf;

			if (open_connection_xia(sa, tx_cinfo) < 0) {
				LOG("Error connecting to initiator");
				rcm->set_message("Error connecting to initiator");
				rcm->set_rc(session::FAILURE);
				return -1;
			}

			name_to_conn[nexthop] = tx_cinfo;
#endif /* XIA */
		} else {
			tx_cinfo = name_to_conn[nexthop];
		}

		tx_cinfo->add_sessions(new_ctx);
		ctx_to_txconn[new_ctx] = tx_cinfo;
LOGF("    Opened connection with initiator on sock %d", tx_cinfo->sockfd());
	} 

		
	// send session info to next hop
	if (send(new_ctx, &pkt) < 0) {
		LOG("Error connecting to or sending session info to next hop");
		rcm->set_message("Error connecting to or sending session info to next hop");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

LOG("    Sent SessionInfo packet to next hop");

	free(rpkt); // TODO: not freed if we hit an error and returned early

	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_send_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_send_msg");

	int sent;
	const session::SSendMsg sendm = msg.s_send();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();


	// Build a SessionPacket
	session::SessionPacket pkt;
	pkt.set_type(session::DATA);
	pkt.set_sender_ctx(ctx);
	session::SessionData *dm = pkt.mutable_data();
	dm->set_data(sendm.data());

	// Send it
	sent = send(ctx, &pkt);

	// Report back how many bytes we sent
	session::SSendRet *retm = reply.mutable_s_send_ret();
	retm->set_bytes_sent(sent);


	if ( sent < 0 ) {
		LOG("Error sending data");
		rcm->set_message("Error sending data");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_recv_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_recv_msg");

	const session::SRecvMsg rm = msg.s_recv();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();

	// pull packet off data Q
	queue<session::SessionPacket*> *dataQ = ctx_to_dataQ[ctx];
	pthread_mutex_t *mutex = ctx_to_dataQ_mutex[ctx];
	pthread_cond_t *cond = ctx_to_dataQ_cond[ctx];
	pthread_mutex_lock(mutex);
	if (dataQ->size() ==0) pthread_cond_wait(cond, mutex);  // wait until there's a message
	session::SessionPacket *pkt = dataQ->front();
	dataQ->pop();
	pthread_mutex_unlock(mutex);


	if ( pkt->type() != session::DATA || !pkt->has_data() || !pkt->data().has_data()) {
		LOG("Received packet did not contain data");
		rcm->set_message("Received packet did not contain data");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	
	// Send back the data
	session::SRecvRet *retm = reply.mutable_s_recv_ret();
	retm->set_data(pkt->data().data());

	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}


int send_reply(int sockfd, struct sockaddr_in *sa, socklen_t sa_size, session::SessionMsg *sm)
{
	int rc = 0;

	assert(sm);

	std::string p_buf;
	sm->SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();
	while (remaining > 0) {
		rc = sendto(sockfd, p, remaining, 0, (struct sockaddr *)sa, sa_size);

		if (rc == -1) {
			LOGF("socket failure: error %d (%s)", errno, strerror(errno));
			break;
		} else {
			remaining -= rc;
			p += rc;
			if (remaining > 0) {
				LOGF("%d bytes left to send", remaining);
#if 1
				// FIXME: click will crash if we need to send more than a 
				// single buffer to get the entire block of data sent. Is 
				// this fixable, or do we have to assume it will always go
				// in one send?
				LOG("click can't handle partial packets");
				rc = -1;
				break;
#endif
			}
		}	
	}

	return  (rc >= 0 ? 0 : -1);
}

void* process_function_wrapper(void* args) {
LOG("BEGIN wrapper");
	struct proc_func_data *data = (struct proc_func_data*)args;

	int rc = data->process_function(data->ctx, *(data->msg), *(data->reply));

	// send the reply that was filled in by the processing function
	if (rc < 0) {
		LOG("Error processing message");
	}
	if (send_reply(data->sockfd, data->sa, data->len, data->reply) < 0) {
		LOG("Error sending reply");
	}

	free(data->sa);
	delete data->msg;
	delete data->reply;
	free(args);

	return NULL;
LOG("END wrapper");
}

int listen() {
	// Open socket to listen on
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		LOGF("error creating socket to listen on: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(procport);

	if (bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sockfd);
		LOGF("bind error: %s", strerror(errno));
		return -1;
	}


	// listen for messages (and process them)
	LOGF("Listening on %d", procport);
	while (true)
	{
		struct sockaddr_in sa;
		socklen_t len = sizeof sa;
		int rc;
		char buf[BUFSIZE];
		unsigned buflen = sizeof(buf);
		memset(buf, 0, buflen);

LOG("Waiting for a new message");
print_sessions();
print_connections();
		if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
			LOGF("error(%d) getting reply data from session process", errno);
		} else {
			// TODO: make this robust to null chars (make into std::string?)
			session::SessionMsg sm;
			sm.ParseFromString(buf);

			// for now we use the sender's port as the context handle
			int ctx = ntohs(sa.sin_port);

			// make a function pointer to store which processing function we want to use
			int (*process_function)(int, session::SessionMsg&, session::SessionMsg&) = NULL;

			switch(sm.type())
			{
				case session::NEW_CONTEXT:
					process_function = &process_new_context_msg;
					//rc = process_new_context_msg(sm.s_new_context(), &sa, reply);
					break;
				case session::INIT:
					process_function = &process_init_msg;
					//rc = process_init_msg(ctx, sm.s_init(), reply);
					break;
				case session::BIND:
					process_function = &process_bind_msg;
					//rc = process_bind_msg(ctx, sm.s_bind(), reply);
					break;
				case session::ACCEPT:
					process_function = &process_accept_msg;
					//rc = process_accept_msg(ctx, sm.s_accept(), reply);
					break;
				case session::SEND:
					process_function  = &process_send_msg;
					//rc = process_send_msg(ctx, sm.s_send(), reply);
					break;
				case session::RECEIVE:
					process_function = &process_recv_msg;
					//rc = process_recv_msg(ctx, sm.s_recv(), reply);
					break;
				default:
					LOG("Unrecognized protobuf message");
			}

			// make a new thread
			pthread_t dummy_handle;
			struct proc_func_data* args = (struct proc_func_data*)malloc(sizeof(struct proc_func_data));
			args->process_function = process_function;
			args->sockfd = sockfd;
			args->sa = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
			memcpy(args->sa, &sa, len);
			args->len = len;
			args->ctx = ctx;
			args->msg = new session::SessionMsg(sm);
			args->reply = new session::SessionMsg();

			int pthread_rc;
			if ( (pthread_rc = pthread_create(&dummy_handle, NULL, process_function_wrapper, (void*)args)) < 0) {
				LOGF("Error creating thread: %s", strerror(pthread_rc));
			}
		}
	}
}




int main(int argc, char *argv[]) {
	getConfig(argc, argv);

	// set sockconf.ini name
	set_conf("xsockconf.ini", sockconf_name.c_str());
	LOGF("Click sockconf name: %s", sockconf_name.c_str());

	listen();
	return 0;
}
