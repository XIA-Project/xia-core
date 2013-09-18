#include "session.pb.h"
#include "utils.hpp"
#include "modules/SessionModule.h"
#include "modules/TransportModule.h"
#include "modules/SSLModule.h"
#include "modules/StackInfo.h"
#include "modules/UserLayerInfo.h"
#include "modules/AppLayerInfo.h"
#include "modules/TransportLayerInfo.h"
#include "modules/NetworkLayerInfo.h"
#include "modules/LinkLayerInfo.h"
#include "modules/PhysicalLayerInfo.h"
#include <map>
#include <queue>
#include <stdio.h>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

using namespace std;

#define DEFAULT_PROCPORT 1989
#define BUFSIZE 65000
#define MTU 1250
#define MOBILITY_CHECK_INTERVAL 1
#define ADU_DELIMITER "\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f"  // 0x1f is the unit separator
#define RECV_BUFFER_LEN 500 // number of *packets* to buffer, not bytes

#define Q_NOT_ALLOCATED 0
#define Q_ALLOCATED 1
#define Q_WATCHED 2


/* LOGGING */

#define V_ERROR 0
#define V_WARNING 1
#define V_INFO 2
#define V_DEBUG 3

#ifdef DEBUG
#define VERBOSITY V_DEBUG
#else
#define VERBOSITY V_DEBUG
#endif

#define LOG(levelstr, color, s) fprintf(stderr, "\033[0;3%dm[ %s ]\033[0m\t[%s:%d (thread %p)]\t%s\n", color, levelstr, __FILE__, __LINE__, (void*)pthread_self(), s)
#define LOGF(levelstr, color, fmt, ...) fprintf(stderr, "\033[0;3%dm[ %s ]\033[0m\t[%s:%d (thread %p)]\t" fmt"\n", color, levelstr, __FILE__, __LINE__, (void*)pthread_self(), __VA_ARGS__) 

#if VERBOSITY >= V_INFO
#define INFO(s) LOG("INFO", 2, s)
#define INFOF(fmt, ...) LOGF("INFO", 2, fmt, __VA_ARGS__)
#else
#define INFO(s)
#define INFOF(fmt, ...)
#endif

#if VERBOSITY >= V_DEBUG
#define DBG(s) LOG("DEBUG", 4, s)
#define DBGF(fmt, ...) LOGF("DEBUG", 4, fmt, __VA_ARGS__)
#else
#define DBG(s)
#define DBGF(fmt, ...)
#endif

#if VERBOSITY >= V_ERROR
#define ERROR(s) LOG("ERROR", 1, s)
#define ERRORF(fmt, ...) LOGF("ERROR", 1, fmt, __VA_ARGS__)
#else
#define ERROR(s)
#define ERRORF(fmt, ...)
#endif

#if VERBOSITY >= V_WARNING
#define WARN(s) LOG("WARNING", 3, s)
#define WARNF(fmt, ...) LOGF("WARNING", 3, fmt, __VA_ARGS__)
#else
#define WARN(s)
#define WARNF(fmt, ...)
#endif



/* FORWARD DECLARATIONS */
void *poll_recv_sock(void *args);
bool is_last_hop(int ctx, string name);
int send(int ctx, session::SessionPacket *pkt);
int send(session::ConnectionInfo *cinfo, session::SessionPacket *pkt);
int migrate_connections();
bool checkQ(queue<session::SessionPacket*> *Q);
map<int, session::ConnectionInfo*> getAllConnections();



#define MULTIPLEX
#define XIA

#ifdef XIA
#include "xia.cpp"
#endif
#ifdef IP
#include "ip.cpp"
#endif




/* DATA STRUCTURES */

struct proc_func_data {
	int (*process_function)(int, session::SessionMsg&, session::SessionMsg&);
	int sockfd;
	struct sockaddr_in *sa;
	socklen_t len;
	int ctx;
	session::SessionMsg *msg;
	session::SessionMsg *reply;
};

struct queue_data {
	queue<session::SessionPacket*> *q;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
	short state;
};

struct context_queues {
	struct queue_data accept;
	struct queue_data data;

	struct queue_data send_buffer;
	unsigned int next_send_seqnum;

	session::SessionPacket* recv_buffer[RECV_BUFFER_LEN];  // TODO: allocate dynamically, since we might not always use it?
	unsigned int recv_buffer_start;
	pthread_mutex_t *recv_buffer_mutex;
	pthread_cond_t *recv_buffer_cond;  // TODO: needed?
	unsigned int next_recv_seqnum;
};



map<unsigned short, session::ContextInfo*> ctx_to_ctx_info;
map<unsigned short, session::SessionInfo*> ctx_to_session_info; 
map<unsigned short, session::ConnectionInfo*> ctx_to_txconn;
map<unsigned short, session::ConnectionInfo*> ctx_to_rxconn;
map<unsigned short, int> ctx_to_listensock;
map<uint32_t, unsigned short> incomingctx_to_myctx; // key: upper 16 = sender's ctx, lower 16 = incoming sockfd
pthread_mutex_t incomingctx_to_myctx_mutex;
map<string, session::ConnectionInfo*> hid_to_conn;
pthread_mutex_t hid_to_conn_mutex;

map<int, pthread_t*> sockfd_to_pthread;  // could be a listen sock or data sock (not both)
map<string, unsigned short> name_to_listen_ctx;
map<unsigned short, struct context_queues*> ctx_to_queues; // TODO: make mutex for this

vector<SessionModule *> session_modules;
UserLayerInfo userInfo;
TransportLayerInfo transportInfo;
NetworkLayerInfo netInfo;
LinkLayerInfo linkInfo;
PhysicalLayerInfo physicalInfo;

pthread_t mobility_thread;

string sockconf_name = "sessiond";
int procport = DEFAULT_PROCPORT;







/* PRINT FUNCTIONS */
void print_contexts() {
	printf("*******************************************************************************\n");
	printf("*                                 CONTEXTS                                    *\n");
	printf("*******************************************************************************\n\n");
	
	map<unsigned short, session::ContextInfo*>::iterator iter;
	for (iter = ctx_to_ctx_info.begin(); iter!= ctx_to_ctx_info.end(); ++iter) {
		session::ContextInfo* ctxInfo = iter->second;

		printf("Context:\t%d\n", iter->first);	

		string state = "UNKNOWN";
		if (ctxInfo->state() == session::ContextInfo::CLOSED)
			state = "CLOSED";
		else if (ctxInfo->state() == session::ContextInfo::LISTEN)
			state = "LISTEN";
		else if (ctxInfo->state() == session::ContextInfo::CONNECTING)
			state = "CONNECTING";
		else if (ctxInfo->state() == session::ContextInfo::ESTABLISHED)
			state = "ESTABLISHED";
		else if (ctxInfo->state() == session::ContextInfo::CLOSING)
			state = "CLOSING";
		printf("State:\t\t%s\n", state.c_str());

		if (ctxInfo->has_my_name()) {
			printf("My name:\t%s", ctxInfo->my_name().c_str());
		}
		printf("\n\n-------------------------------------------------------------------------------\n\n");
	}
}

void print_sessions() {
	printf("*******************************************************************************\n");
	printf("*                                 SESSIONS                                    *\n");
	printf("*******************************************************************************\n\n");
	
	map<unsigned short, session::SessionInfo*>::iterator iter;
	for (iter = ctx_to_session_info.begin(); iter!= ctx_to_session_info.end(); ++iter) {
		session::SessionInfo* sinfo = iter->second;

		printf("Context:\t%d\n", iter->first);	
		printf("Session path:\t");
		for (int i = 0; i < sinfo->session_path_size(); i++) {
			printf("%s  ", sinfo->session_path(i).name().c_str());
		}

		if (sinfo->has_my_name()) {
			printf("\nMy name:\t%s", sinfo->my_name().c_str());
		}
		if (sinfo->has_use_ssl() && sinfo->use_ssl()) {
			printf("\nUse SSL:\tTRUE");
		} else {
			printf("\nUse SSL:\tFALSE");
		}
		switch(sinfo->transport_protocol()) {
			case session::TCP:
				printf("\nTransport:\tTCP");
				break;
			case session::UDP:
				printf("\nTransport:\tUDP");
				break;
			case session::XSP:
				printf("\nTransport:\tXSP");
				break;
			case session::XDP:
				printf("\nTransport:\tXDP");
				break;
		}
		printf("\n\n-------------------------------------------------------------------------------\n\n");
	}
}

void print_connections() {
	printf("*******************************************************************************\n");
	printf("*                               CONNECTIONS                                   *\n");
	printf("*******************************************************************************\n\n");

	map<int, session::ConnectionInfo*> connections = getAllConnections();
	map<int, session::ConnectionInfo*>::iterator iter;
	for (iter = connections.begin(); iter!= connections.end(); ++iter) {
		session::ConnectionInfo *cinfo = iter->second;

		printf("HID:\t\t%s\n", cinfo->hid().c_str());
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


// returns true if context exists and is in the specified state
// returns false otherwise
bool checkContext(int ctx, session::ContextInfo_ContextState state) {
	if (ctx_to_ctx_info.find(ctx) == ctx_to_ctx_info.end())
		return false;

	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];
	if (ctxInfo->state() != state)
		return false;
	
	return true;
}

int getLocalContextForIncomingContext(int incomingCtx, int incomingSock) {
	uint32_t key = incomingCtx << 16;
	key += incomingSock;

	if (incomingctx_to_myctx.find(key) != incomingctx_to_myctx.end())
		return incomingctx_to_myctx[key];
	else
		return -1;
}

void setLocalContextForIncomingContext(int localCtx, int incomingCtx, int incomingSock) {
	pthread_mutex_lock(&incomingctx_to_myctx_mutex);
	uint32_t key = incomingCtx << 16;
	key += incomingSock;
	incomingctx_to_myctx[key] = localCtx;
	pthread_mutex_unlock(&incomingctx_to_myctx_mutex);
}

/*
int removeLocalContextForIncomingContext(int localCtx, int incomingCtx, int incomingSock) {
	pthread_mutex_lock(&incomingctx_to_myctx);
	uint32_t key = incomingCtx << 16;
	key += incomingSock;
	incomingctx_to_myctx[key] = localCtx;
	pthread_mutex_unlock(&incomingctx_to_myctx);
}*/

/**
* @brief Returns a list of all open connections.
*
* @return Map of sockfd -> ConnectionInfo*
*/
map<int, session::ConnectionInfo*> getAllConnections() {
	map<int, session::ConnectionInfo*> connections;
	map<unsigned short, session::ConnectionInfo*>::iterator connIt;
	for (connIt = ctx_to_txconn.begin(); connIt != ctx_to_txconn.end(); ++connIt) {
		if (connections.find(connIt->second->sockfd()) == connections.end()) {
			connections[connIt->second->sockfd()] = connIt->second;
		}
	}
	for (connIt = ctx_to_rxconn.begin(); connIt != ctx_to_rxconn.end(); ++connIt) {
		if (connections.find(connIt->second->sockfd()) == connections.end()) {
			connections[connIt->second->sockfd()] = connIt->second;
		}
	}
	return connections;
}

session::SessionPacket* popQ(struct queue_data *qdata, uint32_t max_bytes) {
	// check that the Q is allocated
	if (qdata->state == Q_NOT_ALLOCATED) {
		ERROR("Popping packet from a queue that is not allocated");
		return NULL;
	}

	queue<session::SessionPacket*> *Q = qdata->q;
	pthread_mutex_t *mutex = qdata->mutex;
	pthread_cond_t *cond = qdata->cond;

	pthread_mutex_lock(qdata->mutex);
	qdata->state = Q_WATCHED;
	if (Q->size() == 0) pthread_cond_wait(cond, mutex);  // wait until there's a message

	// We either got signalled because there's data to pop or because the Q
	// was dealloc'd. Figure out which.
	if (qdata->state == Q_NOT_ALLOCATED) {
		pthread_cond_signal(cond); // signal that we got the message
		pthread_mutex_unlock(mutex);
		return NULL; 
	}

	session::SessionPacket *pkt = Q->front();
	if ( max_bytes > 0 && pkt->data().data().size() > max_bytes) {
		// make a new packet containing the first max_bytes bytes of the data;
		// then remove the bytes from the packet on the Q, but leave it there
		pkt = new session::SessionPacket(*pkt);

		// only take the first max_bytes bytes
		pkt->mutable_data()->mutable_data()->erase(max_bytes, pkt->data().data().size()-max_bytes);

		// only leave behind what we didn't read
		Q->front()->mutable_data()->mutable_data()->erase(0, max_bytes);
	} else {
		Q->pop();
	}

	qdata->state = Q_ALLOCATED;
	pthread_mutex_unlock(mutex);
	return pkt;
}

session::SessionPacket* popDataQ(int ctx, uint32_t max_bytes) {
	return popQ(&(ctx_to_queues[ctx]->data), max_bytes);
}

session::SessionPacket* popAcceptQ(int ctx) {
	return popQ(&(ctx_to_queues[ctx]->accept), -1);
}

int pushQ(session::SessionPacket* pkt, struct queue_data *qdata) {
	// check that the Q is allocated
	if (qdata->state == Q_NOT_ALLOCATED) {
		ERROR("Pushing packet to a queue that is not allocated");
		return -1;
	}

	pthread_mutex_lock(qdata->mutex);
	qdata->q->push(pkt);
	pthread_mutex_unlock(qdata->mutex);
	pthread_cond_signal(qdata->cond);

	return 0;
}

int pushDataQ(int ctx, session::SessionPacket *pkt) {
	return pushQ(pkt, &(ctx_to_queues[ctx]->data));
}

int pushAcceptQ(int ctx, session::SessionPacket *pkt) {
	return pushQ(pkt, &(ctx_to_queues[ctx]->accept));
}

int QSize(struct queue_data *qdata) {
	// check that the Q is allocated
	if (qdata->state == Q_NOT_ALLOCATED) {
		ERROR("Pushing packet to a queue that is not allocated");
		return -1;
	}

	pthread_mutex_lock(qdata->mutex);
	int size = qdata->q->size();
	pthread_mutex_unlock(qdata->mutex);

	return size;
}

int dataQSize(int ctx) {
	return QSize(&(ctx_to_queues[ctx]->data));
}

int acceptQSize(int ctx) {
	return QSize(&(ctx_to_queues[ctx]->accept));
}

void allocateQ(struct queue_data *qdata) {
	// allocate a queue
	qdata->q = new queue<session::SessionPacket*>();
	qdata->state = Q_ALLOCATED;

	// allocate new pthread vars for this data Q
	qdata->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(qdata->mutex, NULL);
	qdata->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	pthread_cond_init(qdata->cond, NULL);
}

void deallocateQ(struct queue_data *qdata) {

	queue<session::SessionPacket*> *Q = qdata->q;
	pthread_mutex_t *mutex = qdata->mutex;
	pthread_cond_t *cond = qdata->cond;

	pthread_mutex_lock(mutex);
	
	// mark the Q as dealloc'd and signal anyone waiting on this Q
	bool watched = qdata->state == Q_WATCHED;
	if (watched) {
		pthread_cond_signal(cond); // signal watched Q is going away
		pthread_cond_wait(cond, mutex); // wait until they get the message
	}

	// dealloc the Q
	qdata->state = Q_NOT_ALLOCATED;
	delete Q;

	pthread_mutex_unlock(mutex);

	// free the mutex and condition var
	free(mutex);
	free(cond);
}


int alloc_context_queues(int ctx) {

	if (ctx_to_ctx_info.find(ctx) == ctx_to_ctx_info.end() ) {
		ERRORF("Could not find context %d in ctx_to_ctx_info.", ctx);
		return -1;
	}
	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];

	// allocate the struct
	struct context_queues *queues = (struct context_queues*)malloc(sizeof(struct context_queues));
	queues->accept.state = Q_NOT_ALLOCATED;
	queues->data.state = Q_NOT_ALLOCATED;
	queues->send_buffer.state = Q_NOT_ALLOCATED;

	// add to mapping
	ctx_to_queues[ctx] = queues;

	// always make accept Q
	allocateQ(&(queues->accept));

	// only make data queue, send/recv buffers if this is a session context
	if (ctxInfo->state() == session::ContextInfo::CONNECTING ||
	    ctxInfo->state() == session::ContextInfo::ESTABLISHED ) {
		allocateQ(&(queues->data));
		allocateQ(&(queues->send_buffer));

		queues->next_send_seqnum = 0;
		queues->next_recv_seqnum = 0;

		queues->recv_buffer_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(queues->recv_buffer_mutex, NULL);
		queues->recv_buffer_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
		pthread_cond_init(queues->recv_buffer_cond, NULL);
	}
	
	return 0;
}

int dealloc_context_queues(int ctx) {
	
	if (ctx_to_ctx_info.find(ctx) == ctx_to_ctx_info.end() ) {
		ERRORF("Could not find context %d in ctx_to_ctx_info.", ctx);
		return -1;
	}
	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];
	struct context_queues *queues = ctx_to_queues[ctx];

	// deallocate acceptQ
	deallocateQ(&(queues->accept));

	// if this is a SESSION context, dealloc dataQ, send/recv buffers
	if (ctxInfo->state() == session::ContextInfo::CONNECTING ||
	    ctxInfo->state() == session::ContextInfo::ESTABLISHED ) {
		deallocateQ(&(queues->data));
		deallocateQ(&(queues->send_buffer));

		pthread_mutex_lock(queues->recv_buffer_mutex);
		free(queues->recv_buffer_cond);
		pthread_mutex_unlock(queues->recv_buffer_mutex);
		free(queues->recv_buffer_mutex);
	}

	ctx_to_queues.erase(ctx);
	free(queues);

	return 0;
}

int startPollingSocket(int sock, void*(polling_function)(void*), void* args) {
	pthread_t *t = (pthread_t*)malloc(sizeof(pthread_t));
	int pthread_rc;
	if ( (pthread_rc = pthread_create(t, NULL, polling_function, args)) >= 0)
		sockfd_to_pthread[sock] = t;
	return pthread_rc;
}

int stopPollingSocket(int sock) {
	int rc = 1;
	pthread_t *t = sockfd_to_pthread[sock];
	
	// no need to kill ourselves; this could happen if poll_recv_sock got a
	// TEARDOWN msg. poll_recv_sock will exit after closeSession call completes,
	// so all we need to do here is remove state
	if (*t != pthread_self()) {
		rc = pthread_cancel(*t);
		if (rc != 0) {
			ERRORF("Error cancelling listen thread for sock %d: %s", sock, strerror(rc));
		}
		rc = pthread_join(*t, NULL);
		if (rc != 0) {
			ERRORF("Error waiting for listen thread to terminate (sock %d): %s", sock, strerror(rc));
		}
	}

	free(sockfd_to_pthread[sock]);
	sockfd_to_pthread.erase(sock);
	return rc;
}

// Removes the specified context from the connection and, if no one else is
// using the connection, closes it
bool closeConnection(int ctx, session::ConnectionInfo *cinfo) {
	bool closed_conn = false;
		
	pthread_mutex_lock(&hid_to_conn_mutex);  // TODO: make a mutex for each connection
	// remove ctx from the connections list of sessions
	int found = -1;
	for (int i = 0; i < cinfo->sessions_size(); i++) {
		if (ctx == cinfo->sessions(i)) {
			found = i;
			break;
		}
	}

	if (found == -1) {
		WARNF("Did not find context %d in connection", ctx);
		pthread_mutex_unlock(&hid_to_conn_mutex);
		return closed_conn;
	}

	// swap found ctx with the last ctx, then remove last
	// (that's how the protobuf API makes us do it)
	cinfo->mutable_sessions()->SwapElements(found, cinfo->sessions_size()-1);
	cinfo->mutable_sessions()->RemoveLast();

	// If no one else is using this connection, close it
	if (cinfo->sessions_size() == 0) {
		INFOF("Closing connection to: %s", cinfo->hid().c_str());
		closed_conn = true;
		
		// remove mapping
		hid_to_conn.erase(cinfo->hid()); // TODO: when we have indidivual mutexes, still lock whole thing here

		// stop polling the socket
		if (stopPollingSocket(cinfo->sockfd()) < 0) {
			ERRORF("Error closing the polling thread for socket %d", cinfo->sockfd());
		}

		// close the socket
		if (closeSock(cinfo->sockfd()) < 0) {
			ERRORF("Error closing socket %d", cinfo->sockfd());
		}
		
		// remove mapping
		delete cinfo;
	}
	
	pthread_mutex_unlock(&hid_to_conn_mutex);
	return closed_conn;
}

bool closeSession(int ctx) {
DBGF("Closing session %d", ctx);
	bool kill_self = false;
	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx]; // check that ctx exists
	session::ContextInfo::ContextState state = ctxInfo->state();
	
	if (state != session::ContextInfo::CLOSED) {
		ctxInfo->set_state(session::ContextInfo::CLOSING); // just in case someone tries to use this ctx while we're closing it

		if (state == session::ContextInfo::LISTEN) {

			stopPollingSocket(ctx_to_listensock[ctx]);

		} else if (state == session::ContextInfo::ESTABLISHED ||
				   state == session::ContextInfo::CONNECTING) {

			// first forward close message to next hop (if i'm not last)
			if (!is_last_hop(ctx, ctx_to_session_info[ctx]->my_name())) {
DBGF("Ctx %d    Sending TEARDOWN to next hop", ctx);
				session::SessionPacket *pkt = new session::SessionPacket();
				pkt->set_type(session::TEARDOWN);
				pkt->set_sender_ctx(ctx);
				if (send(ctx, pkt) < 0) {
					ERROR("Error sending teardown message to next hop");
				}
				delete pkt;
			}


			// then tear down state on this node
			closeConnection(ctx, ctx_to_txconn[ctx]);
			kill_self = closeConnection(ctx, ctx_to_rxconn[ctx]); // this thread is the rxconn thread
			ctx_to_rxconn.erase(ctx);
			ctx_to_txconn.erase(ctx);
			
			delete ctx_to_session_info[ctx];
			ctx_to_session_info.erase(ctx);
		} else {
			ERRORF("Unknown context type for context %d", ctx);
		}
	}

	dealloc_context_queues(ctx);

	delete ctxInfo;  // TODO: should we delete context, or just set to closed?
	ctx_to_ctx_info.erase(ctx);

print_contexts();
print_sessions();
print_connections();
	return kill_self;
}



void infocpy(const session::SessionInfo *from, session::SessionInfo *to) {
	for (int i = 0; i < from->session_path_size(); i++) {
		session::SessionInfo_ServiceInfo *serviceInfo = to->add_session_path();
		serviceInfo->set_name(from->session_path(i).name());
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

	if (from->has_initiator_ctx()) {
		to->set_initiator_ctx(from->initiator_ctx());
	}

	if (from->has_new_lasthop_conn()) {
		to->set_new_lasthop_conn(from->new_lasthop_conn());
	}

	if (from->has_use_ssl()) {
		to->set_use_ssl(from->use_ssl());
	}
	if (from->has_transport_protocol()) {
		to->set_transport_protocol(from->transport_protocol());
	}
}

// counts the number of application endpoints in a session
int session_hop_count(const session::SessionInfo *info) {
	return info->session_path_size();
}

int session_hop_count(int ctx) {
	session::SessionInfo *info = ctx_to_session_info[ctx];
	return session_hop_count(info);
}

// name is last hop if it is second to last in the session path
bool is_last_hop(const session::SessionInfo *info, string name) {
	return info->session_path(info->session_path_size()-2).name() == name;
}

bool is_last_hop(int ctx, string name) {
	session::SessionInfo *info = ctx_to_session_info[ctx];
	return is_last_hop(info, name);
}

string get_neighborhop_name(string my_name, const session::SessionInfo *info, bool next) {

	// Look for my_name in the session path
	int found_index = -1;
	for (int i = 0; i < info->session_path_size(); i++) {
		if (info->session_path(i).name() == my_name) {
			found_index = i;
			break;
		}
	}

	int hop = next ? 1 : -1;

	if (found_index == -1) {
		WARNF("My name not found in session path: %s", my_name.c_str());
		return "NOT FOUND";
	} else {
		// move one hop in the appropriate direction
		int neighbor_index = found_index + hop;

		// wrap around if necessary
		if (neighbor_index == -1) neighbor_index = info->session_path_size()-1;
		else if (neighbor_index == info->session_path_size()) neighbor_index = 0;

		return info->session_path(neighbor_index).name();
	}
}

string get_nexthop_name(string name, const session::SessionInfo *info) {
	return get_neighborhop_name(name, info, true);
}

string get_prevhop_name(string name, const session::SessionInfo *info) {
	return get_neighborhop_name(name, info, false);
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



void configure_session(session::SessionInfo *sinfo, AppLayerInfo &appInfo) {
	
	// Step through the session modules one by one, allowing each to configure
	// the session by makings its decision.
	vector<SessionModule*>::iterator iter;
	for (iter = session_modules.begin(); iter != session_modules.end(); iter++) {
		(*iter)->decide(sinfo, userInfo, appInfo, transportInfo, netInfo, linkInfo, physicalInfo);
	}
}

bool trigger_breakpoint(Breakpoint breakpoint, struct breakpoint_context *context, void *rv) {

	// Step through the session modules one by one triggering the breakpoint
	bool performFollowingOp = true;   // e.g., the "send" in a PreSend breakpoint
	vector<SessionModule*>::iterator iter;
	for (iter = session_modules.begin(); iter != session_modules.end(); iter++) {
		if (!((*iter)->breakpoint(breakpoint, context, rv)))
			performFollowingOp = false;
	}

	free(context->args);
	free(context);
	return performFollowingOp;
}



/**
* @brief Opens a transport connection and sends setup_packet, if supplied. Also
*	initiates SSL handshake if needed.
*
* @param sinfo
* @param addr_buf
* @param cinfo
* @param setup_packet INFO or MIGRATE
*
* @return 
*/
int open_connection(const session::SessionInfo *sinfo, const string *addr_buf, session::ConnectionInfo *cinfo, session::SessionPacket *setup_packet) {

	// Transfer relevant session information to the connection (e.g., so future 
	// connect/send/recv calls know wheter to use TCP or UDP, SSL or not, etc.)
	cinfo->set_use_ssl(sinfo->use_ssl());
	cinfo->set_transport_protocol(sinfo->transport_protocol());


	// Open the conneciton
	int ret;
	if ( (ret = openConnectionToAddr(addr_buf, cinfo)) >= 0) {
		hid_to_conn[cinfo->hid()] = cinfo;

		// send a setup packet (INFO or MIGRATE) if one was supplied
		if (setup_packet != NULL) {
			if (send(cinfo, setup_packet) < 0) {
				ERROR("Error sending setup packet over new connection");
				return -1;
			}
		}

		// do SSL handshake before we start doing receives
		if (sinfo->use_ssl()) {
			connectSSL(cinfo);
		}

		// start a thread reading this socket
		if ( (ret = startPollingSocket(cinfo->sockfd(), poll_recv_sock, (void*)cinfo) < 0) ) {
			ERRORF("Error creating recv sock thread: %s", strerror(ret));
			return -1;
		}
	}

	return ret;

}

int open_connection(const session::SessionInfo *sinfo, string name, session::ConnectionInfo *cinfo, session::SessionPacket *setup_packet) {
	return open_connection(sinfo, getAddrForName(name), cinfo, setup_packet);
}

int open_connection_to_nexthop(int ctx, session::SessionPacket *setup_packet) {

	int rc = 0;
	session::ConnectionInfo *cinfo;


	// If we already have a connection, don't open a new one
	if ( ctx_to_txconn.find(ctx) != ctx_to_txconn.end() )
		return rc;


	// There isn't a transport conn for this session+hop yet
#ifdef MULTIPLEX
	// If no one else has opened a connection to this name, open one
	string hid = getHIDForName(get_nexthop_name(ctx));
	pthread_mutex_lock(&hid_to_conn_mutex); // lock so 2 threads don't try to create same conn
	if ( hid_to_conn.find(hid) == hid_to_conn.end() ) {
#endif /* MULTIPLEX */
		cinfo = new session::ConnectionInfo();
		rc = open_connection(ctx_to_session_info[ctx], get_nexthop_name(ctx), cinfo, setup_packet);
#ifdef MULTIPLEX
	} else {
		cinfo = hid_to_conn[hid];
	}
	pthread_mutex_unlock(&hid_to_conn_mutex);
#endif /* MULTIPLEX */

	if (rc < 0) {
		return rc;
	}

	// now add this context to the connection so it doesn't get "garbage collected"
	// if all the other sessions using it close
	cinfo->add_sessions(ctx);
	cinfo->set_my_name(ctx_to_ctx_info[ctx]->my_name());

	ctx_to_txconn[ctx] = cinfo;
	return rc;
}

int get_conn_for_context(int ctx, map<unsigned short, session::ConnectionInfo*> *ctx_to_conn, string &hopname, session::ConnectionInfo **cinfo) {
	int rc = 0;

	if ( ctx_to_conn->find(ctx) != ctx_to_conn->end() ) {
		// This session already has a transport conn
		*cinfo = (*ctx_to_conn)[ctx];
		return (*cinfo)->sockfd();
	} else {
		WARN("Shouldn't trigger this code");
			// There isn't a transport conn for this session+hop yet
	#ifdef MULTIPLEX
			// If no one else has opened a connection to this name, open one
			string hid = getHIDForName(hopname);
			pthread_mutex_lock(&hid_to_conn_mutex); // lock so 2 threads don't try to create same conn
			if ( hid_to_conn.find(hid) == hid_to_conn.end() ) {
	#endif /* MULTIPLEX */
				*cinfo = new session::ConnectionInfo();
				rc = open_connection(ctx_to_session_info[ctx], hopname, *cinfo, NULL);
	#ifdef MULTIPLEX
			} else {
				*cinfo = hid_to_conn[hid];
			}
			pthread_mutex_unlock(&hid_to_conn_mutex);
	#endif /* MULTIPLEX */
	
			if (rc < 0) {
				return rc;
			}
	
			// now add this context to the connection so it doesn't get "garbage collected"
			// if all the other sessions using it close
			(*cinfo)->add_sessions(ctx);
			(*cinfo)->set_my_name(ctx_to_ctx_info[ctx]->my_name());
	
			(*ctx_to_conn)[ctx] = *cinfo;
	
			// TODO: check that the connection is of the correct type (TCP vs UDP etc)
	
			return (*cinfo)->sockfd();
	}
}

int get_txconn_for_context(int ctx, session::ConnectionInfo **cinfo) {
	string nexthop = get_nexthop_name(ctx);
	return get_conn_for_context(ctx, &ctx_to_txconn, nexthop, cinfo);
}

int get_rxconn_for_context(int ctx, session::ConnectionInfo **cinfo) {
	string prevhop = get_prevhop_name(ctx);
	return get_conn_for_context(ctx, &ctx_to_rxconn, prevhop, cinfo);
}


// sends to arbitrary transport connection
int send(session::ConnectionInfo *cinfo, session::SessionPacket *pkt) {
	int sent = -1;

	string p_buf;
	assert(pkt);
	pkt->SerializeToString(&p_buf);
	int len = p_buf.size();
	const char *buf = p_buf.c_str();

	// (*) PreSendBreakpoint
	bool doSend = trigger_breakpoint(kSendPreSend, new breakpoint_context(NULL, cinfo, new send_args(buf, &len)), &sent);

	if (doSend) {
		if ( (sent = sendBuffer(cinfo, buf, len)) < 0) {
			ERRORF("Send error %d on socket %d, dest hid %s: %s", errno, cinfo->sockfd(), cinfo->hid().c_str(), strerror(errno));
		}
	}
		
	// (*) PostSendBreakpoint
	trigger_breakpoint(kSendPostSend, new breakpoint_context(NULL, cinfo, new send_args(buf, &len)), &sent);
	
	return sent;
}

// sends to next hop
int send(int ctx, session::SessionPacket *pkt) {
	session::ConnectionInfo *cinfo = NULL;
	get_txconn_for_context(ctx, &cinfo);
	return send(cinfo, pkt);
}

int recv(session::ConnectionInfo *cinfo, session::SessionPacket *pkt) {
	int received = -1;
	char buf[BUFSIZE];
	int len = BUFSIZE;
	
	// (*) PreRecvBreakpoint
	bool doRecv = trigger_breakpoint(kRecvPreRecv, new breakpoint_context(NULL, cinfo, new recv_args(buf, &len)), &received);

	if (doRecv) {
		if ((received = recvBuffer(cinfo, buf, len)) < 0) {
			ERRORF("Receive error %d on socket %d: %s", errno, cinfo->sockfd(), strerror(errno));
			return received;
		}
	}
		
	// (*) PostRecvBreakpoint
	trigger_breakpoint(kRecvPostRecv, new breakpoint_context(NULL, cinfo, new recv_args(buf, &len)), &received);

	string bufstr(buf, received);
	if (!pkt->ParseFromString(bufstr)) {
		ERROR("Error parsing protobuf packet");
		return -1;
	}
	return received;
}

int swap_sockets_for_connection(session::ConnectionInfo *cinfo, int oldsock, int newsock) {
	// stop the thread that was polling the old socket
	stopPollingSocket(oldsock);

	// close old socket
	if (closeSock(oldsock) < 0) {
		ERRORF("Error closing old transport connection to %s", cinfo->hid().c_str());
		return -1;
	}

	// replace the socket we were using with this name with newsock
	cinfo->set_sockfd(newsock);

	// correct incomingctx_to_myctx mapping (since incoming sockfd changed)
	pthread_mutex_lock(&incomingctx_to_myctx_mutex);
	for (int i = 0; i < cinfo->sessions_size(); i++) {
		int myctx = cinfo->sessions(i);

		vector<uint32_t> keysToChange;
		map<uint32_t, unsigned short>::iterator iter;
		for (iter = incomingctx_to_myctx.begin(); iter!= incomingctx_to_myctx.end(); ++iter) {
			int32_t incoming_sock = iter->first & 0xFFFF;

			if (myctx == iter->second && incoming_sock == oldsock) {
				keysToChange.push_back(iter->first);
			}
		}

		vector<uint32_t>::iterator keyIter;
		for (keyIter = keysToChange.begin(); keyIter != keysToChange.end(); ++keyIter) {
			uint32_t oldKey = *keyIter;
			uint32_t newKey = oldKey - oldsock + newsock;
			incomingctx_to_myctx.erase(oldKey);
			incomingctx_to_myctx[newKey] = myctx;
		}
	}
	pthread_mutex_unlock(&incomingctx_to_myctx_mutex);
		
	// start a thread reading new socket
	int rc;
	if ( (rc = startPollingSocket(newsock, poll_recv_sock, (void*)cinfo) < 0) ) {
		ERRORF("Error creating recv sock thread: %s", strerror(rc));
	}

	return 1;
}

// called when we switch networks. close any existing transport connections
// and restart them.
int migrate_connections() {
	INFO("Migrating existing transport connections.");
	int rc = 1;
	
	map<int, session::ConnectionInfo*> connections = getAllConnections();
	map<int, session::ConnectionInfo*>::iterator iter;
	for (iter = connections.begin(); iter!= connections.end(); ++iter) {
	
		session::ConnectionInfo *cinfo = iter->second;
		int oldsock = cinfo->sockfd();

		// open a new one
		if (openConnectionToAddr(&cinfo->addr(), cinfo) < 0) {
			ERRORF("Error opening new transport connection to %s", cinfo->hid().c_str());
			rc = -1;
			continue;
		}
		
		// swap old socket for new one
		swap_sockets_for_connection(cinfo, oldsock, cinfo->sockfd()); 

		// tell the other side who we are
		session::SessionPacket pkt;
		pkt.set_type(session::MIGRATE);
		session::SessionMigrate *m = pkt.mutable_migrate();
		m->set_sender_name(cinfo->my_name());
		if (send(cinfo, &pkt) < 0) {
			ERRORF("Error sending MIGRATE packet to %s", cinfo->hid().c_str());
			rc = -1;
			continue;
		}
	}

	return rc;
}








/* POLLING FUNCTIONS */

// TODO: Good way to report failures from these functions?
void * poll_listen_sock(void * args) {

	int ctx = *((int*)args);   // TOOD: check this
	free(args);
	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];
	int listen_sock = ctx_to_listensock[ctx];
	if (listen_sock < 0) {
		ERRORF("poll_listen_sock: bad socket: %d", listen_sock);
		return NULL;
	}

	while(true)
	{
		int new_rxsock = -1;
		
		// accept connection on listen sock
		if ( (new_rxsock = acceptSock(listen_sock)) < 0 ) {
			ERRORF("Error accepting new connection on socket %d for listen context %d: %s", listen_sock, ctx, strerror(errno));
		}
		DBGF("    Accepted new connection on sockfd %d", ctx_to_listensock[ctx]);
	
	
		session::ConnectionInfo *rx_cinfo = new session::ConnectionInfo();
		rx_cinfo->set_ssl_ctx_ptr(ctxInfo->ssl_ctx_ptr());
		rx_cinfo->set_sockfd(new_rxsock);
		rx_cinfo->set_hid(getHIDForSocket(new_rxsock));
		rx_cinfo->set_addr(*getRemoteAddrForSocket(new_rxsock));
#ifdef XIA
		rx_cinfo->set_transport_protocol(session::XSP);  // FIXME: we don't know yet, but recv will complain if this isn't set.
#else
		rx_cinfo->set_transport_protocol(session::TCP);  
#endif
	
		
		// receive first message (using new_ctx, not ctx) and
		// make sure it's a session info or lasthop handshake msg
		session::SessionPacket *rpkt = new session::SessionPacket();
		if ( recv(rx_cinfo, rpkt) < 0 ) {
			ERRORF("Error receiving on listen sock for context %d", ctx);
		}

		switch (rpkt->type())
		{
			case session::SETUP:  // someone new wants to talk to us
			{
				if (!rpkt->has_info()) {
					ERROR("SETUP packet didn't contain session info");
					continue;
				}

				rx_cinfo->set_use_ssl(rpkt->info().use_ssl());
				rx_cinfo->set_transport_protocol(rpkt->info().transport_protocol()); // TODO: a bit late for this... we already did an accept and a streaming receive...

				// (*) PostReceiveSYNBreakpoint
				trigger_breakpoint(kAcceptPostReceiveSYN, new breakpoint_context(NULL, rx_cinfo, NULL), NULL);

#ifdef MULTIPLEX
				// save transport connection for others to use
				hid_to_conn[rx_cinfo->hid()] = rx_cinfo;
#endif /* MULTIPLEX */

				// Save rx_info in the session info so accepter can get to it
				rpkt->mutable_info()->set_rx_cinfo(&rx_cinfo, sizeof(session::ConnectionInfo*));
	
				// Add session info pkt to acceptQ
				pushAcceptQ(ctx, rpkt);

				// start a thread reading this socket
				int rc;
				if ( (rc = startPollingSocket(rx_cinfo->sockfd(), poll_recv_sock, (void*)rx_cinfo) < 0) ) {
					ERRORF("Error creating recv sock thread: %s", strerror(rc));
				}

				break;
			}
			case session::MIGRATE:  // someone we were already talking to moved
			{
				DBGF("Received a MIGRATE message from: %s", rpkt->migrate().sender_name().c_str());
				if (!rpkt->has_migrate()) {
					ERROR("MIGRATE packet didn't contain migrate info");
					continue;
				}

				// find the old connection
				delete rx_cinfo; // there's not really a new connection after all
				rx_cinfo = hid_to_conn[rx_cinfo->hid()]; //TODO: handle not found
				// TODO: does the migrate message still need to send the sender's name?

				swap_sockets_for_connection(rx_cinfo, rx_cinfo->sockfd(), new_rxsock);


				break;
			}
			default:
				WARNF("poll_listen_sock received a packet of unknown or unexpected type (ctx %d)", ctx);
		}
	}

	return NULL;
}

// TODO: take in how much data should be read and buffer unwanted data somewhere
void *poll_recv_sock(void *args) {
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); // exit right away if cancelled

	session::ConnectionInfo *cinfo = (session::ConnectionInfo*)args;
	int sock = cinfo->sockfd();
	int rc;
	
	if (sock < 0) {
		ERRORF("poll_recv_sock: bad socket: %d", sock);
		return NULL;
	}

	while(true)
	{
		session::SessionPacket *rpkt = new session::SessionPacket();

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); // cancel is possible while waiting
		rc = recv(cinfo, rpkt);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); // don't cancel while we process a packet

		if (rc < 0) {
			ERRORF("Error receiving on data sock %d", sock);
			continue;
		}
	

		// get the context this packet belongs to
		int ctx = getLocalContextForIncomingContext(rpkt->sender_ctx(), sock);

		// process packet according to its type
		switch(rpkt->type())
		{
			case session::SETUP:
			{
				int listen_ctx;

				string my_name = get_nexthop_name(rpkt->info().my_name(), &(rpkt->info()));
				if (name_to_listen_ctx.find(my_name) != name_to_listen_ctx.end()) {  // first try name -> listen ctx
					listen_ctx = name_to_listen_ctx[my_name];

					// Check that ctx is an initialized listen context
					if (!checkContext(listen_ctx, session::ContextInfo::LISTEN)) {
						ERROR("Received SETUP packet but listen context not initialized");
						break;
					}
				} else if (rpkt->info().has_initiator_ctx()) {  // next try initiator ctx -> listen ctx
					listen_ctx = rpkt->info().initiator_ctx();

					// Check that ctx is in CONNECTING state (this is the case when a session
					// initiator is waiting for the final SETUP connreq packet)
					if (!checkContext(listen_ctx, session::ContextInfo::CONNECTING)) {
						ERRORF("Received SETUP packet but context %d is not in CONNECTING state", listen_ctx);
						break;
					}
				} else {
					ERROR("Could not find listen context for SETUP packet");
					break;
				}
				
				// Save rx_info in the session info so accepter can get to it
				rpkt->mutable_info()->set_rx_cinfo(&cinfo, sizeof(session::ConnectionInfo*));
				
				// Add session info pkt to acceptQ
				pushAcceptQ(listen_ctx, rpkt);
				break;
			}
			case session::DATA:
			{
				// Check that ctx is in ESTABLISHED state
				if (!checkContext(ctx, session::ContextInfo::ESTABLISHED)) {
					ERRORF("Received a data packet for context %d, which is not in the ESTABLISHED state (sender ctx %d)", ctx, rpkt->sender_ctx());
					break;
				}

				// Add data pkt to dataQ
				pushDataQ(ctx, rpkt);
				break;
			}
			case session::TEARDOWN:
			{
				bool kill_self = closeSession(ctx);
				if (kill_self) return NULL;
				delete rpkt;
				break;
			}
			default:
				WARNF("Received a packet of unknown type on socket %d", sock);
		}
	}

	return NULL;
}










/* API MESSAGE PROCESSING  FUNCTIONS */

int process_new_context_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
DBG("BEGIN process_new_context_msg");
	(void)msg; // msg currently unused

	// allocate context info
	session::ContextInfo *ctxInfo = new session::ContextInfo();
	ctx_to_ctx_info[ctx] = ctxInfo;

	// return success
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	rcm->set_rc(session::SUCCESS);

	return 0;
}

int process_init_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
DBG("BEGIN process_init_msg");
	session::SInitMsg im = msg.s_init();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	// check that this context exists and is in the CLOSED state
	if (!checkContext(ctx, session::ContextInfo::CLOSED)) {
		ERRORF("Context %d does not exist or is not in the CLOSED state.", ctx);
		rcm->set_message("Context does not exist or is not in the CLOSED state.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	// allocate session state and data queue for this session
	session::SessionInfo* info = new session::SessionInfo();
	ctx_to_session_info[ctx] = info;
	
	// set context status to CONNECTING
	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];
	ctxInfo->set_state(session::ContextInfo::CONNECTING);
	ctxInfo->set_my_name(info->my_name());
	
	// grab the session attributes & encapsulate in AppLayerInfo object
	AppLayerInfo appInfo(im.attributes());

	// parse path into session state msg
	vector<string> pathNames = split(im.session_path(), ',');
	if (pathNames.size() <= 0) {
		ERROR("Session path is empty");
		rcm->set_message("Session path is empty");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	for (vector<string>::iterator it = pathNames.begin(); it != pathNames.end(); ++it) {
		session::SessionInfo_ServiceInfo *serviceInfo = info->add_session_path();
		serviceInfo->set_name(trim(*it));
	}

	// set my name and add me to the session path
	char my_name[30];
	gethostname(my_name, 30);  // TODO: use something unique?
	info->set_my_name(my_name);
	session::SessionInfo_ServiceInfo *serviceInfo = info->add_session_path();
	serviceInfo->set_name(info->my_name());

	// allocate dataQ, acceptQ (might not be used), send buffer, & recv buffer
	alloc_context_queues(ctx);


	// MAKE DECISIONS
	configure_session(info, appInfo);


	session::ConnectionInfo *rx_cinfo = NULL;
	string *my_addr_buf = NULL;
	string prevhop = get_prevhop_name(ctx);
	string prevHID = getHIDForName(prevhop);
	string* prevaddr = getAddrForName(prevhop);
	int lsock = -1; // might not get used
	bool two_party = (session_hop_count(ctx) == 2);


	// If necessary, make a socket for the last hop to connect to
	pthread_mutex_lock(&hid_to_conn_mutex);
#ifdef MULTIPLEX
	bool connection_exists = !( hid_to_conn.find(prevHID) == hid_to_conn.end() );
	if (!connection_exists && !two_party) {  // NOT ALREADY A CONNECTION
#else
	if (!two_party) { 
#endif /* MULTIPLEX */
DBGF("Ctx %d    No connection exists, making rx_sock", ctx);
		
		// store this rx-transport-conn-to-be so others connecting to the same
		// lasthop don't make their own rx conns (actual sock filled in below)
		rx_cinfo = new session::ConnectionInfo();
		rx_cinfo->set_hid(prevHID);
		rx_cinfo->set_addr(*prevaddr);
		rx_cinfo->set_my_name(info->my_name());
		rx_cinfo->add_sessions(ctx);
		rx_cinfo->set_use_ssl(info->use_ssl());
		rx_cinfo->set_transport_protocol(info->transport_protocol());

		hid_to_conn[prevHID] = rx_cinfo;
		pthread_mutex_unlock(&hid_to_conn_mutex);
		
		// make a socket to accept connection from last hop
		lsock = bindNewSocket(rx_cinfo, &my_addr_buf);
		if (lsock < 0) {
			ERROR("Error binding to random address");
			rcm->set_message("Error binding to random address");
			rcm->set_rc(session::FAILURE);
			pthread_mutex_unlock(&hid_to_conn_mutex);
			return -1;
		}
	
		info->set_new_lasthop_conn(true);  // last hop should open new conn to me
DBG("    Made socket to accept synack from last hop");
	
	} else {
DBGF("Ctx %d    Found an exising connection, waiting for synack in acceptQ", ctx);
		// we already alloc'd an acceptQ

		// tell the last hop our context # so things get linked up correctly
		// when we get a synack on this end
		info->set_initiator_ctx(ctx);

		// add this session to the connection (do this now to protect against
		// the connection being closed while we send around the session info)
		if (!two_party) {
			rx_cinfo = hid_to_conn[prevHID];
			rx_cinfo->add_sessions(ctx);
			my_addr_buf = getLocalAddrForSocket(rx_cinfo->sockfd());
		}
		pthread_mutex_unlock(&hid_to_conn_mutex);
	}
		
	if (!two_party) {
		// store addr in session info
		info->set_my_addr(*my_addr_buf);
		info->set_initiator_addr(*my_addr_buf);
	}
	delete my_addr_buf;

	// send session info to next hop
	session::SessionPacket pkt;
	pkt.set_type(session::SETUP); 
	pkt.set_sender_ctx(ctx);
	session::SessionInfo *newinfo = pkt.mutable_info();// TODO: better way?
	infocpy(info, newinfo);
	if ( open_connection_to_nexthop(ctx, &pkt) < 0) {
		rcm->set_message("Error sending session info to next hop");
		rcm->set_rc(session::FAILURE);
	}
DBGF("Ctx %d    Sent connection request to next hop", ctx);


	session::ConnectionInfo *tx_cinfo;
	get_txconn_for_context(ctx, &tx_cinfo);
	if (tx_cinfo->use_ssl()) {
		connectSSL(tx_cinfo);
	}


	if (two_party) {  // rxconn and txconn are the same
		get_txconn_for_context(ctx, &rx_cinfo);
		rx_cinfo->add_sessions(ctx);  // redundant...
	}


	session::SessionPacket *rpkt = new session::SessionPacket();
	int rxsock = -1;
#ifdef MULTIPLEX
	if (!connection_exists && !two_party) {  // NOT ALREADY A CONNECTION
#else
	if (!two_party) { 
#endif /* MULTIPLEX */
		// accept connection on listen sock
		if ( (rxsock = acceptSock(lsock)) < 0 ) {
			ERRORF("Error accepting connection from last hop on context %d", ctx);
			rcm->set_message("Error accepting connection from last hop");
			rcm->set_rc(session::FAILURE);
			return -1;
		}
		closeSock(lsock);
DBGF("Ctx %d    Accepted a new connection on rxsock %d", ctx, rxsock);

		// fill in the connection info we didn't have above
		rx_cinfo->set_sockfd(rxsock);


		// listen for "synack" -- receive and check incoming session info msg
		if ( recv(rx_cinfo, rpkt) < 0) {
			ERROR("Error receiving synack session info msg");
			rcm->set_message("Error receiving synack session info msg");
			rcm->set_rc(session::FAILURE);
			return -1;
		} 
	
		// (*) PostReceiveSYNBreakpoint
		trigger_breakpoint(kAcceptPostReceiveSYN, new breakpoint_context(NULL, rx_cinfo, NULL), NULL);

		// start a thread reading this socket
		int rc;
		if ( (rc = startPollingSocket(rx_cinfo->sockfd(), poll_recv_sock, (void*)rx_cinfo) < 0) ) {
			ERRORF("Error creating recv sock thread: %s", strerror(rc));
			return -1;
		}
	} else {
		// get synack session info from accept Q
		rpkt = popAcceptQ(ctx);

		if (rpkt == NULL) {
			ERROR("Trying to listen on a closed context");
			rcm->set_message("Trying to listen on a closed context");
			rcm->set_rc(session::FAILURE);
			return -1;
		}
		
		rxsock = rx_cinfo->sockfd();

		// Note: since this connection already existed, it should already have SSL
		// set up (if we're using SSL). BUT, if an SSL and a non-SSL connection both
		// try to share the same underlying connection, we don't support that now!
	}

	ctx_to_rxconn[ctx] = rx_cinfo;
	
	
	// TODO: check that this is the correct session info pkt
	if (rpkt->type() != session::SETUP) {
		ERROR("Expecting final synack, but packet was not a SESSION_INFO msg.");
		rcm->set_message("Expecting final synack, but packet was not a SESSION_INFO msg.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	string sender_name = rpkt->info().my_name();
DBGF("Ctx %d    Got final synack from: %s (sender ctx: %d)", ctx, sender_name.c_str(), rpkt->sender_ctx());


	
	// store incoming ctx mapping
	setLocalContextForIncomingContext(ctx, rpkt->sender_ctx(), rxsock);

	// set session state to ESTABLISHED
	ctxInfo->set_state(session::ContextInfo::ESTABLISHED);


print_contexts();
print_sessions();
print_connections();
	
	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_bind_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
DBG("BEGIN process_bind_msg");
	const session::SBindMsg bm = msg.s_bind();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	// check that this context exists and is in the CLOSED state
	if (!checkContext(ctx, session::ContextInfo::CLOSED)) {
		ERRORF("Context %d does not exist or is not in the CLOSED state.", ctx);
		rcm->set_message("Context does not exist or is not in the CLOSED state.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	
	INFOF("    Binding to name: %s", bm.name().c_str());
	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];

	// bind to random app ID
	string *addr_buf = NULL;
	int sock = bindNewSocket(ctxInfo, &addr_buf);
	if (sock < 0) {
		ERROR("Error binding to random address");
		rcm->set_message("Error binding to random address");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	

	// store port as this context's listen sock. This context does not have
	// tx or rx sockets
	ctx_to_listensock[ctx] = sock;

	DBGF("    Registering name: %s", bm.name().c_str());
	// register name
    if (registerName(bm.name(), addr_buf) < 0) {
    	ERRORF("error registering name: %s\n", bm.name().c_str());
		rcm->set_message("Error registering name");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	DBG("    Registered name");

	// if everything worked, store name and addr in context info
	ctxInfo->set_state(session::ContextInfo::LISTEN);
	ctxInfo->set_my_name(bm.name());
	ctxInfo->set_my_addr(*addr_buf);
	
	// store a mapping of name -> listen_ctx
	name_to_listen_ctx[bm.name()] = ctx;

	// initialize a queue to store incoming Connection Requests
	// (these might arrive on the accept socket or on another socket,
	// since the session layer shares transport connections among sessions)
	// (Since this context has type LISTEN, alloc_context_queues will only allocate
	// an acceptQ, not a dataQ, send buffer, or recv buffer)
	alloc_context_queues(ctx);

	// kick off a thread draining the listen socket into the acceptQ
	int rc;
	int *ctxptr = (int*)malloc(sizeof(int)); // TODO: better way?
	*ctxptr = ctx;
	if ( (rc = startPollingSocket(sock, poll_listen_sock, (void*)ctxptr) < 0) ) {
		ERRORF("Error creating listen sock thread: %s", strerror(rc));
		rcm->set_message("Error creating listen sock thread");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	
	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_accept_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
DBG("BEGIN process_accept_msg");
	
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	const session::SAcceptMsg am = msg.s_accept();
	uint32_t new_ctx = am.new_ctx();

	// check that this context exists and is in the LISTEN state
	if (!checkContext(ctx, session::ContextInfo::LISTEN)) {
		ERROR("Context was not an initialized listen context");
		rcm->set_message("Context was not an initialized listen context");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	session::ContextInfo *listenCtxInfo = ctx_to_ctx_info[ctx];

	// pull message from acceptQ
	session::SessionPacket *rpkt = popAcceptQ(ctx);
	if (rpkt == NULL) {
		ERROR("Trying to listen on a closed context");
		rcm->set_message("Trying to listen on a closed context");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	assert(rpkt);


	// First make sure we have a connection to the next hop.
	session::ConnectionInfo *tx_cinfo = NULL;

	// If i'm the last hop, the initiator doesn't have a name in the name service.
	// So, we need to use the DAG that was supplied by the initiator and build the
	// tx_conn ourselves. (Unless we already have a connection open with them.)
	// BUT: if it's just me and the initiator, we already have a connection.
	bool two_party = (session_hop_count(&(rpkt->info())) == 2 );
	if ( is_last_hop(&(rpkt->info()), listenCtxInfo->my_name()) && !two_party) {
		string nexthop = get_nexthop_name(listenCtxInfo->my_name(), &(rpkt->info()));

		if (rpkt->info().new_lasthop_conn()) {
DBGF("    Ctx %d    I'm the last hop; connecting to initiator with supplied addr", new_ctx);
			tx_cinfo = new session::ConnectionInfo();

			// TODO: don't dup this code!
			session::SessionInfo *sinfo = new session::SessionInfo();
			infocpy(&(rpkt->info()), sinfo);
			sinfo->set_my_name(listenCtxInfo->my_name());
			sinfo->clear_my_addr();
			ctx_to_session_info[new_ctx] = sinfo;

			session::SessionPacket pkt;
			pkt.set_type(session::SETUP); 
			pkt.set_sender_ctx(new_ctx);
			session::SessionInfo *newinfo = pkt.mutable_info();// TODO: better way?
			infocpy(sinfo, newinfo);

			if (open_connection(&(rpkt->info()), &(rpkt->info().initiator_addr()), tx_cinfo, &pkt) < 0) {
				ERROR("Error connecting to initiator");
				rcm->set_message("Error connecting to initiator");
				rcm->set_rc(session::FAILURE);
				return -1;
			}

			pthread_mutex_lock(&hid_to_conn_mutex);
			hid_to_conn[tx_cinfo->hid()] = tx_cinfo;
			tx_cinfo->add_sessions(new_ctx);
			pthread_mutex_unlock(&hid_to_conn_mutex);

		} else {

#ifdef MULTIPLEX
			// check for an existing connection
			string nextHID = getHIDForAddr(&(rpkt->info().initiator_addr()));
			pthread_mutex_lock(&hid_to_conn_mutex);
			if ( hid_to_conn.find(nextHID) == hid_to_conn.end() ) {
DBGF("    Ctx %d    I'm the last hop; waiting for a connection to initiator. Putting ConnReq back on Q", new_ctx);
				// there isn't one, so we'll put the ConnReq back on the accecptQ
				// so we can process it again later once we've gotten a ConnReq
				// with the initiator's address
				pushAcceptQ(ctx, rpkt);
				pthread_mutex_unlock(&hid_to_conn_mutex);
				
				// Try again (hopefully someone else has either added the ConnReq
				// we want to the acceptQ or they've processed it themselves and
				// already created the connection we need)
				return process_accept_msg(ctx, msg, reply);
			}

			// we're good to go!
			tx_cinfo = hid_to_conn[nextHID];
			tx_cinfo->add_sessions(new_ctx);
			pthread_mutex_unlock(&hid_to_conn_mutex);
#else
			ERROR("Last hop. Initiator did not supply an address and we're not multiplexing");
			rcm->set_message("Last hop. Initiator did not supply an address and we're not multiplexing");
			rcm->set_rc(session::FAILURE);
			return -1;
#endif /* MULTIPLEX */
		}

		ctx_to_txconn[new_ctx] = tx_cinfo;
	} 



	// store a copy of the session info, updating my_name and my_addr
	//session::SessionInfo *sinfo = new session::SessionInfo(rpkt->info());
	session::SessionInfo *sinfo = new session::SessionInfo();
	infocpy(&(rpkt->info()), sinfo);
	sinfo->set_my_name(listenCtxInfo->my_name());
	sinfo->clear_my_addr();
	ctx_to_session_info[new_ctx] = sinfo;

	// allocate context info for the new session context
	session::ContextInfo *ctxInfo = new session::ContextInfo();
	ctxInfo->set_my_name(listenCtxInfo->my_name());
	ctxInfo->set_state(session::ContextInfo::ESTABLISHED); // TODO: should be CONNECTING?
	ctx_to_ctx_info[new_ctx] = ctxInfo;

	// allocate dataQ, acceptQ (might not be used), send buffer, & recv buffer
	alloc_context_queues(new_ctx);
	
	// get the rx transport connection
	if (rpkt->info().has_rx_cinfo()) {
		session::ConnectionInfo *rx_cinfo;  // TODO: malloc this?
		memcpy(&rx_cinfo, rpkt->info().rx_cinfo().data(), sizeof(session::ConnectionInfo*));
		ctx_to_rxconn[new_ctx] = rx_cinfo;
		rx_cinfo->add_sessions(new_ctx);

		// TODO: should this be only if multiplexing is off? I think it's OK as it is.
		if (two_party) {  // this is the only case where we didn't set tx_cinfo above
			tx_cinfo = rx_cinfo;
			ctx_to_txconn[new_ctx] = tx_cinfo;
			tx_cinfo->add_sessions(new_ctx);
		}

	} else {
		ERROR("ConnReq wasn't tagged with incoming connection.");
		rcm->set_message("No multiplexing, but ConnReq wasn't tagged with incoming connection.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	
	// store incoming ctx mapping
	setLocalContextForIncomingContext(new_ctx, rpkt->sender_ctx(), ctx_to_rxconn[new_ctx]->sockfd());


	// connect to next hop
	session::SessionPacket pkt;
	pkt.set_type(session::SETUP); 
	pkt.set_sender_ctx(new_ctx);
	session::SessionInfo *newinfo = pkt.mutable_info();// TODO: better way?
	infocpy(sinfo, newinfo);
	

		
	// send session info to next hop
	if ( open_connection_to_nexthop(new_ctx, &pkt) < 0) {
		ERROR("Error connecting to or sending session info to next hop");
		rcm->set_message("Error connecting to or sending session info to next hop");
		rcm->set_rc(session::FAILURE);
	}

DBGF("    Ctx %d    Sent SessionInfo packet to next hop", new_ctx);

	delete rpkt; // TODO: not freed if we hit an error and returned early


	// Get a fresh pointer to tx_cinfo (if we didn't have to create one above,
	// it's still NULL)
	get_txconn_for_context(new_ctx, &tx_cinfo);
	
	// if this is 2-party and we already did acceptSSL on this connection (in 
	// poll_listen_sock), then connectSSL will just return and not hurt anything
	if (tx_cinfo->use_ssl()) {
		connectSSL(tx_cinfo);
	}

	ctxInfo->set_state(session::ContextInfo::ESTABLISHED);

print_contexts();
print_sessions();
print_connections();

	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_send_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
//DBG("BEGIN process_send_msg");

	uint32_t sent = 0;
	const session::SSendMsg sendm = msg.s_send();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();

	// Check that ctx exists and is ESTABLISHED
	if (!checkContext(ctx, session::ContextInfo::ESTABLISHED)) {
		ERRORF("Context %d does not exist or is not in an ESTABLISHED state.", ctx);
		rcm->set_message("Context does not exist or is not in an ESTABLISHED state.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}


	// Add the ADU_DELIMITER to the data to send (and make sure it didn't
	// accidentally appear in the data)
	if (sendm.data().find(ADU_DELIMITER) != sendm.data().npos) {
		ERROR("WARNING: ADU_DELIMITER found in data to send!");
	}
	string data_str(sendm.data() + ADU_DELIMITER);

	// Build a SessionPacket
	session::SessionPacket pkt;
	pkt.set_type(session::DATA);
	pkt.set_sender_ctx(ctx);
	session::SessionData *dm = pkt.mutable_data();


	// send MTU bytes at a time until we've sent the whole message
	// for now, assume that protobur overhead is 10 bytes TODO: get actual size
	uint32_t overhead = 10;
	while (sent < data_str.size()) {
		dm->set_data(data_str.substr(sent, MTU-overhead)); // grab next chunk of bytes
		if ( (send(ctx, &pkt)) < 0 ) {
			ERROR("Error sending data");
			rcm->set_message("Error sending data");
			rcm->set_rc(session::FAILURE);
			return -1;
		}

		sent += dm->data().size();
	}


	// Report back how many bytes we sent (don't include delimiter)
	sent -= strlen(ADU_DELIMITER);
	session::SSendRet *retm = reply.mutable_s_send_ret();
	retm->set_bytes_sent(sent);


	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_recv_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
//DBG("BEGIN process_recv_msg");

	const session::SRecvMsg rm = msg.s_recv();
	uint32_t max_bytes = rm.bytes_to_recv();

	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	// Check that ctx exists and is ESTABLISHED
	if (!checkContext(ctx, session::ContextInfo::ESTABLISHED)) {
		ERRORF("Context %d does not exist or is not in an ESTABLISHED state.", ctx);
		rcm->set_message("Context does not exist or is not in an ESTABLISHED state.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	bool waitForADU = rm.wait_for_adu();
	bool foundADU = false;

	// pull packet off data Q
	session::SessionPacket *pkt = NULL;
	string data;

	// keep pulling data while:  there's data in the Q and we have less than max_bytes and (but only if we're not waiting for a full ADU)
	//                           OR: we're waiting for a full ADU and we have less than max_bytes
	do {
		delete pkt; // free current packet before blowing away the pointer
		pkt = popDataQ(ctx, max_bytes - data.size());

		if ( pkt == NULL ) {
			ERROR("Popped packet was NULL");
			rcm->set_message("Popped packet was NULL");
			rcm->set_rc(session::FAILURE);
			return -1;
		}
		if ( pkt->type() != session::DATA || !pkt->has_data() || !pkt->data().has_data()) {
			ERROR("Received packet did not contain data");
			rcm->set_message("Received packet did not contain data");
			rcm->set_rc(session::FAILURE);
			return -1;
		}
		
		data += pkt->data().data();

		// check for ADU delimiter
		size_t pos = data.find(ADU_DELIMITER);
		if (pos != data.npos) {
			data.erase(pos, strlen(ADU_DELIMITER)); // remove delim char
			foundADU = true; // we've got it!
		}
	}
	while ( (data.size() < max_bytes && !waitForADU && dataQSize(ctx) > 0) 
			|| (waitForADU && !foundADU && data.size() < max_bytes) 
			|| (data.size() == 0) ) ;

	// Send back the data
	session::SRecvRet *retm = reply.mutable_s_recv_ret();
	retm->set_data(data);
	
	delete pkt;
	
	if (waitForADU && data.size() > max_bytes) {
		ERROR("Not enough room in buffer for full ADU. Returning max_bytes.");
		rcm->set_message("Not enough room in buffer for full ADU. Returning max_bytes.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_close_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
DBG("BEGIN process_close_msg");
	
	const session::SCloseMsg cm = msg.s_close();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();

	closeSession(ctx);
	
	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_check_for_data_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
DBG("BEGIN process_check_for_data_msg");
	
	const session::SCloseMsg cm = msg.s_close();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	// Check that ctx exists and is ESTABLISHED
	if (!checkContext(ctx, session::ContextInfo::ESTABLISHED)) {
		ERRORF("Context %d does not exist or is not in an ESTABLISHED state.", ctx);
		rcm->set_message("Context does not exist or is not in an ESTABLISHED state.");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	session::SCheckDataRet *cdret = reply.mutable_s_check_data_ret();
	cdret->set_data_available( (dataQSize(ctx) > 0) ); 

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
			ERRORF("socket failure: error %d (%s)", errno, strerror(errno));
			break;
		} else {
			remaining -= rc;
			p += rc;
		}	
	}

	return  (rc >= 0 ? 0 : -1);
}

void* process_function_wrapper(void* args) {
	struct proc_func_data *data = (struct proc_func_data*)args;

	int rc = data->process_function(data->ctx, *(data->msg), *(data->reply));

	// send the reply that was filled in by the processing function
	if (rc < 0) {
		ERROR("Error processing message");
	}
	if (send_reply(data->sockfd, data->sa, data->len, data->reply) < 0) {
		ERROR("Error sending reply");
	}

	free(data->sa);
	delete data->msg;
	delete data->reply;
	free(args);

	return NULL;
}


int listen() {
	// Open socket to listen on
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		ERRORF("error creating socket to listen on: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(procport);

	if (bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sockfd);
		ERRORF("bind error: %s", strerror(errno));
		return -1;
	}


	// listen for messages (and process them)
	INFOF("Listening on %d", procport);
	while (true)
	{
		struct sockaddr_in sa;
		socklen_t len = sizeof sa;
		int rc;
		char buf[BUFSIZE];
		unsigned buflen = sizeof(buf);
		memset(buf, 0, buflen);

		if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
			ERRORF("error(%d) getting reply data from session process", errno);
		} else {
			string p_buf(buf, rc); // need to make into a std::string first in case of null chars
			session::SessionMsg sm;
			sm.ParseFromString(p_buf);

			// for now we use the sender's port as the context handle
			int ctx = ntohs(sa.sin_port);

			// make a function pointer to store which processing function we want to use
			int (*process_function)(int, session::SessionMsg&, session::SessionMsg&) = NULL;

			switch(sm.type())
			{
				case session::NEW_CONTEXT:
					process_function = &process_new_context_msg;
					break;
				case session::INIT:
					process_function = &process_init_msg;
					break;
				case session::BIND:
					process_function = &process_bind_msg;
					break;
				case session::ACCEPT:
					process_function = &process_accept_msg;
					break;
				case session::SEND:
					process_function  = &process_send_msg;
					break;
				case session::RECEIVE:
					process_function = &process_recv_msg;
					break;
				case session::CLOSE:
					process_function = &process_close_msg;
					break;
				case session::CHECK_FOR_DATA:
					process_function = &process_check_for_data_msg;
					break;
				default:
					WARN("Unrecognized protobuf message");
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
			if ( (pthread_rc = pthread_create(&dummy_handle, NULL, process_function_wrapper, (void*)args)) != 0) {
				ERRORF("Error creating thread: %s", strerror(pthread_rc));
			}
			if ( (pthread_rc = pthread_detach(dummy_handle)) != 0 ) {
				ERRORF("Error detaching thread: %s", strerror(pthread_rc));
			}
		}
	}
}




int main(int argc, char *argv[]) {
	getConfig(argc, argv);

#ifdef XIA
	// set sockconf.ini name
	set_conf("xsockconf.ini", sockconf_name.c_str());
	INFOF("Click sockconf name: %s", sockconf_name.c_str());
#endif /* XIA */

#ifdef IP
	SSL_load_error_strings();
	SSL_library_init();
#endif /* IP */
			
	// Start a thread watching for mobility
	int pthread_rc;
	if ( (pthread_rc = pthread_create(&mobility_thread, NULL, mobility_daemon, NULL)) != 0) {
		ERRORF("Error creating thread: %s", strerror(pthread_rc));
	}

	// Initializes some mutexes
	pthread_mutex_init(&hid_to_conn_mutex, NULL);
	pthread_mutex_init(&incomingctx_to_myctx_mutex, NULL);

	// Initialize Session Modules
#ifdef XIA
	session_modules.push_back(new TransportModuleXIA());
	session_modules.push_back(new SSLModuleXIA());
#else
	session_modules.push_back(new TransportModuleIP());
	session_modules.push_back(new SSLModuleIP());
#endif

	listen();
	return 0;
}
