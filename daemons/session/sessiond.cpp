#include "session.pb.h"
#include "utils.hpp"
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

#define Q_ALLOCATED 1
#define Q_WATCHED 2



#define DEBUG

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: INFO  %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: INFO  " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif
#define ERROR(s) fprintf(stderr, "\033[0;31m%s:%d: ERROR  %s\n\033[0m", __FILE__, __LINE__, s)
#define ERRORF(fmt, ...) fprintf(stderr, "\033[0;31m%s:%d: ERROR  " fmt"\n\033[0m", __FILE__, __LINE__, __VA_ARGS__) 



/* FORWARD DECLARATIONS */
void *poll_recv_sock(void *args);
bool is_last_hop(int ctx, string name);
int send(int ctx, session::SessionPacket *pkt);
int migrate_connections();
bool checkQ(queue<session::SessionPacket*> *Q);



#define XIA

#ifdef XIA
#include "xia.cpp"
#endif
#ifdef IP
#include "ip.cpp"
#endif




/* DATA STRUCTURES */
map<unsigned short, session::ContextInfo*> ctx_to_ctx_info;
map<unsigned short, session::SessionInfo*> ctx_to_session_info; 
map<unsigned short, session::ConnectionInfo*> ctx_to_txconn;
map<unsigned short, session::ConnectionInfo*> ctx_to_rxconn;
map<unsigned short, int> ctx_to_listensock;
map<uint32_t, unsigned short> incomingctx_to_myctx; // key: upper 16 = sender's ctx, lower 16 = incoming sockfd
pthread_mutex_t incomingctx_to_myctx_mutex;
map<string, session::ConnectionInfo*> name_to_conn;
pthread_mutex_t name_to_conn_mutex;

map<int, pthread_t*> sockfd_to_pthread;  // could be a listen sock or data sock (not both)

map<string, unsigned short> name_to_listen_ctx;
map<unsigned short, queue<session::SessionPacket*>* > listen_ctx_to_acceptQ;
map<unsigned short, pthread_mutex_t*> ctx_to_acceptQ_mutex;
map<unsigned short, pthread_cond_t*> ctx_to_acceptQ_cond;

map<unsigned short, queue<session::SessionPacket*>* > ctx_to_dataQ;
map<unsigned short, pthread_mutex_t*> ctx_to_dataQ_mutex;
map<unsigned short, pthread_cond_t*> ctx_to_dataQ_cond;

map<queue<session::SessionPacket*>*, short> QState; // keeps track of which Q's are allocated

pthread_t mobility_thread;

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






/* PRINT FUNCTIONS */
void print_contexts() {
	printf("*******************************************************************************\n");
	printf("*                                 CONTEXTS                                    *\n");
	printf("*******************************************************************************\n\n");
	
	map<unsigned short, session::ContextInfo*>::iterator iter;
	for (iter = ctx_to_ctx_info.begin(); iter!= ctx_to_ctx_info.end(); ++iter) {
		session::ContextInfo* ctxInfo = iter->second;

		printf("Context:\t%d\n", iter->first);	

		string type = "UNASSIGNED";
		if (ctxInfo->type() == session::ContextInfo::LISTEN)
			type = "LISTEN";
		else if (ctxInfo->type() == session::ContextInfo::SESSION)
			type = "SESSION";
		printf("Type:\t\t%s\n", type.c_str());

		printf("Initialized:\t%s", ctxInfo->initialized() ? "YES" : "NO");

		if (ctxInfo->has_my_name()) {
			printf("\nMy name:\t%s", ctxInfo->my_name().c_str());
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

// returns true if context exists, is initialized, and is of the specified type
// returns false otherwise
bool checkContext(int ctx, session::ContextInfo_ContextType type) {
	if (ctx_to_ctx_info.find(ctx) == ctx_to_ctx_info.end())
		return false;

	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];
	if (!ctxInfo->initialized() || ctxInfo->type() != type)
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

session::SessionPacket* popQ(queue<session::SessionPacket*> *Q, pthread_mutex_t *mutex, pthread_cond_t *cond, uint32_t max_bytes) {
	pthread_mutex_lock(mutex);
	QState[Q] = Q_WATCHED;
	if (Q->size() == 0) pthread_cond_wait(cond, mutex);  // wait until there's a message

	// We either got signalled because there's data to pop or because the Q
	// was dealloc'd. Figure out which.
	if (!checkQ(Q)) {
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

	QState[Q] = Q_ALLOCATED;
	pthread_mutex_unlock(mutex);
	return pkt;
}

session::SessionPacket* popDataQ(int ctx, uint32_t max_bytes) {
	return popQ(ctx_to_dataQ[ctx], ctx_to_dataQ_mutex[ctx], ctx_to_dataQ_cond[ctx], max_bytes);
}

session::SessionPacket* popAcceptQ(int ctx) {
	return popQ(listen_ctx_to_acceptQ[ctx], ctx_to_acceptQ_mutex[ctx], ctx_to_acceptQ_cond[ctx], -1);
}

void pushQ(session::SessionPacket* pkt, queue<session::SessionPacket*> *Q, pthread_mutex_t *mutex, pthread_cond_t *cond) {
		pthread_mutex_lock(mutex);
		Q->push(pkt);
		pthread_mutex_unlock(mutex);
		pthread_cond_signal(cond);
}

void pushDataQ(int ctx, session::SessionPacket *pkt) {
	pushQ(pkt, ctx_to_dataQ[ctx], ctx_to_dataQ_mutex[ctx], ctx_to_dataQ_cond[ctx]);
}

void pushAcceptQ(int ctx, session::SessionPacket *pkt) {
	pushQ(pkt, listen_ctx_to_acceptQ[ctx], ctx_to_acceptQ_mutex[ctx], ctx_to_acceptQ_cond[ctx]);
}

void allocateQ(int ctx, map<unsigned short, queue<session::SessionPacket*>* > *QMap,
						map<unsigned short, pthread_mutex_t*> *mutexMap,
						map<unsigned short, pthread_cond_t*> *condMap) {

	// allocate a queue
	queue<session::SessionPacket*> *Q = new queue<session::SessionPacket*>();
	(*QMap)[ctx] = Q;
	QState[Q] = Q_ALLOCATED;

	// allocate new pthread vars for this data Q
	pthread_mutex_t *mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutex, NULL);
	(*mutexMap)[ctx] = mutex;
	pthread_cond_t *cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	pthread_cond_init(cond, NULL);
	(*condMap)[ctx] = cond;
}

void allocateDataQ(int ctx) {
	allocateQ(ctx, &ctx_to_dataQ, &ctx_to_dataQ_mutex, &ctx_to_dataQ_cond);
}

void allocateAcceptQ(int ctx) {
	allocateQ(ctx, &listen_ctx_to_acceptQ, &ctx_to_acceptQ_mutex, &ctx_to_acceptQ_cond);
}

void deallocateQ(int ctx, map<unsigned short, queue<session::SessionPacket*>* > *QMap,
						  map<unsigned short, pthread_mutex_t*> *mutexMap,
						  map<unsigned short, pthread_cond_t*> *condMap) {

	queue<session::SessionPacket*> *Q = (*QMap)[ctx];
	pthread_mutex_t *mutex = (*mutexMap)[ctx];
	pthread_cond_t *cond = (*condMap)[ctx];

	pthread_mutex_lock(mutex);
	
	// mark the Q as dealloc'd and signal anyone waiting on this Q
	bool watched = QState[Q] == Q_WATCHED;
	QState.erase(Q);
	if (watched) {
		pthread_cond_signal(cond); // signal watched Q is going away
		pthread_cond_wait(cond, mutex); // wait until they get the message
	}

	// dealloc the Q
	delete Q;
	QMap->erase(ctx);

	pthread_mutex_unlock(mutex);

	// free the mutex and condition var
	free(mutex);
	mutexMap->erase(ctx);
	free(cond);
	condMap->erase(ctx);
}

void deallocateDataQ(int ctx) {
	deallocateQ(ctx, &ctx_to_dataQ, &ctx_to_dataQ_mutex, &ctx_to_dataQ_cond);
}

void deallocateAcceptQ(int ctx) {
	deallocateQ(ctx, &listen_ctx_to_acceptQ, &ctx_to_acceptQ_mutex, &ctx_to_acceptQ_cond);
}

bool checkQ(queue<session::SessionPacket*> *Q) {
	return QState.find(Q) != QState.end();
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
		
	pthread_mutex_lock(&name_to_conn_mutex);  // TODO: make a mutex for each connection
	// remove ctx from the connections list of sessions
	int found = -1;
	for (int i = 0; i < cinfo->sessions_size(); i++) {
		if (ctx == cinfo->sessions(i)) {
			found = i;
			break;
		}
	}

	if (found == -1) {
		ERRORF("Error: did not find context %d in connection", ctx);
		pthread_mutex_unlock(&name_to_conn_mutex);
		return closed_conn;
	}

	// swap found ctx with the last ctx, then remove last
	// (that's how the protobuf API makes us do it)
	cinfo->mutable_sessions()->SwapElements(found, cinfo->sessions_size()-1);
	cinfo->mutable_sessions()->RemoveLast();

	// If no one else is using this connection, close it
	if (cinfo->sessions_size() == 0) {
		LOGF("Closing connection to: %s", cinfo->name().c_str());
		closed_conn = true;
		
		// free state and remove mapping
		name_to_conn.erase(cinfo->name()); // TODO: when we have indidivual mutexes, still lock whole thing here
		delete cinfo;

		// stop polling the socket
		if (stopPollingSocket(cinfo->sockfd()) < 0) {
			ERRORF("Error closing the polling thread for socket %d", cinfo->sockfd());
		}

		// close the socket
		if (closeSock(cinfo->sockfd()) < 0) {
			ERRORF("Error closing socket %d", cinfo->sockfd());
		}
	}
	
	pthread_mutex_unlock(&name_to_conn_mutex);
	return closed_conn;
}

bool closeSession(int ctx) {
LOGF("Closing session %d", ctx);
	bool kill_self = false;
	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];
	
	if (ctxInfo->initialized()) {
		ctxInfo->set_initialized(false); // just in case someone tries to use this ctx while we're closing it

		if (ctxInfo->type() == session::ContextInfo::LISTEN) {

			stopPollingSocket(ctx_to_listensock[ctx]);
			deallocateAcceptQ(ctx);

		} else if (ctxInfo->type() == session::ContextInfo::SESSION) {

			// first forward close message to next hop (if i'm not last)
			if (!is_last_hop(ctx, ctx_to_session_info[ctx]->my_name())) {
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
			
			deallocateDataQ(ctx);

			delete ctx_to_session_info[ctx];
			ctx_to_session_info.erase(ctx);
		} else {
			ERRORF("Unknown context type for context %d", ctx);
		}
	}

	delete ctxInfo;
	ctx_to_ctx_info.erase(ctx);

print_contexts();
print_sessions();
print_connections();
	return kill_self;
}



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

	if (from->has_initiator_ctx()) {
		to->set_initiator_ctx(from->initiator_ctx());
	}
}

// counts the number of application endpoints in a session
int session_hop_count(int ctx) {
	session::SessionInfo *info = ctx_to_session_info[ctx];
	return info->forward_path_size() + info->return_path_size();
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
		
	// special treatment if there are only two parties
	if ( info->forward_path_size() + info->return_path_size() == 2) {
		if (found_index != -1)
			return info->return_path(0).name();
		else
			return info->forward_path(0).name();
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




int open_connection(string name, session::ConnectionInfo *cinfo) {
	
	int ret;
	if ( (ret = openConnectionToName(name, cinfo)) >= 0) {
		cinfo->set_name(name);
		name_to_conn[name] = cinfo;

		// start a thread reading this socket
		if ( (ret = startPollingSocket(cinfo->sockfd(), poll_recv_sock, (void*)cinfo) < 0) ) {
			ERRORF("Error creating recv sock thread: %s", strerror(ret));
			return -1;
		}
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
		pthread_mutex_lock(&name_to_conn_mutex); // lock so 2 threads don't try to create same conn
		if ( name_to_conn.find(nexthop) == name_to_conn.end() ) {
			*cinfo = new session::ConnectionInfo();
			if ( open_connection(nexthop, *cinfo) < 0 ) {
				pthread_mutex_unlock(&name_to_conn_mutex);
				return -1;
			}
		} else {
			*cinfo = name_to_conn[nexthop];
		}
		pthread_mutex_unlock(&name_to_conn_mutex);

			
		// now add this context to the connection so it doesn't get "garbage collected"
		// if all the other sessions using it close
		(*cinfo)->add_sessions(ctx);  // TODO: remove when session closes
		(*cinfo)->set_my_name(ctx_to_ctx_info[ctx]->my_name());

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
		(*cinfo)->set_my_name(ctx_to_ctx_info[ctx]->my_name());

		ctx_to_rxconn[ctx] = *cinfo;

		// TODO: check that the connection is of the correct type (TCP vs UDP etc)

		return (*cinfo)->sockfd();
	}
}

// sends to arbitrary transport connection
int send(session::ConnectionInfo *cinfo, session::SessionPacket *pkt) {
	int sent = -1;

	string p_buf;
	assert(pkt);
	pkt->SerializeToString(&p_buf);
	int len = p_buf.size();
	const char *buf = p_buf.c_str();

	if ( (sent = sendBuffer(cinfo, buf, len)) < 0) {
		ERRORF("Send error %d on socket %d, dest name %s: %s", errno, cinfo->sockfd(), cinfo->name().c_str(), strerror(errno));
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
	int received = -1;
	char buf[BUFSIZE];

	if ((received = recvBuffer(cinfo, buf, BUFSIZE)) < 0) {
		ERRORF("Receive error %d on socket %d: %s", errno, cinfo->sockfd(), strerror(errno));
		return received;
	}


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
		ERRORF("Error closing old transport connection to %s", cinfo->name().c_str());
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
	LOG("Migrating existing transport connections.");
	int rc = 1;
	
	map<string, session::ConnectionInfo*>::iterator iter;
	for (iter = name_to_conn.begin(); iter!= name_to_conn.end(); ++iter) {
		session::ConnectionInfo *cinfo = iter->second;

		int oldsock = cinfo->sockfd();

		// open a new one
		if (openConnectionToName(cinfo->name(), cinfo) < 0) {
			ERRORF("Error opening new transport connection to %s", iter->first.c_str());
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
			ERRORF("Error sending MIGRATE packet to %s", iter->first.c_str());
			rc = -1;
			continue;
		}
	}

	return rc;
}








/* POLLING FUNCTIONS */

// TODO: Good way to report failures from these functions?
void * poll_listen_sock(void * args) {
LOG("BEGIN poll_listen_sock");

	int ctx = *((int*)args);   // TOOD: check this
	free(args);
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
			//rcm->set_message("Error accpeting new connection on listen context");
			//rcm->set_rc(session::FAILURE);
			//return -1;
		}
	LOGF("    Accepted new connection on sockfd %d", ctx_to_listensock[ctx]);
	
	
		session::ConnectionInfo *rx_cinfo = new session::ConnectionInfo();
		rx_cinfo->set_sockfd(new_rxsock);
	
		
		// receive first message (using new_ctx, not ctx) and
		// make sure it's a session info or lasthop handshake msg
		session::SessionPacket *rpkt = new session::SessionPacket();
		if ( recv(rx_cinfo, rpkt) < 0 ) {
			ERRORF("Error receiving on listen sock for context %d", ctx);
			//rcm->set_message("Error receiving on new connection");
			//rcm->set_rc(session::FAILURE);
			//return -1;
		}

		switch (rpkt->type())
		{
			case session::SETUP:  // someone new wants to talk to us
			{
				if (!rpkt->has_info()) {
					ERROR("SETUP packet didn't contain session info");
					continue;
				}

				// Get the prevhop's name so we can store this transport connection
				string prevhop = rpkt->info().my_name();
				rx_cinfo->set_name(prevhop);
				name_to_conn[prevhop] = rx_cinfo;
	
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
LOGF("Received a MIGRATE message from: %s", rpkt->migrate().sender_name().c_str());
				if (!rpkt->has_migrate()) {
					ERROR("MIGRATE packet didn't contain migrate info");
					continue;
				}

				// find the old connection
				delete rx_cinfo; // there's not really a new connection after all
				rx_cinfo = name_to_conn[rpkt->migrate().sender_name()]; //TODO: handle not found

				swap_sockets_for_connection(rx_cinfo, rx_cinfo->sockfd(), new_rxsock);


				break;
			}
			default:
				ERROR("poll_listen_sock received a packet of unknown or unexpected type");
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

					// Check that ctx is an initialized session context
					if (!checkContext(listen_ctx, session::ContextInfo::SESSION)) {
						ERROR("Received SETUP packet but session context not initialized");
						break;
					}
				} else {
					ERROR("Could not find listen context for SETUP packet");
					break;
				}
				
				
				// Add session info pkt to acceptQ
				pushAcceptQ(listen_ctx, rpkt);
				break;
			}
			case session::DATA:
			{
				// Check that ctx is an initialized session context
				if (!checkContext(ctx, session::ContextInfo::SESSION)) {
					ERRORF("Received a data packet for context %d, which is not an initialized session context (sender ctx %d)", ctx, rpkt->sender_ctx());
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
				ERRORF("Received a packet of unknown type on socket %d", sock);
		}
	}

	return NULL;
}










/* API MESSAGE PROCESSING  FUNCTIONS */

int process_new_context_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_new_context_msg");
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
LOG("BEGIN process_init_msg");
	session::SInitMsg im = msg.s_init();
LOGF("    Initiating a session from %s", im.my_name().c_str());
LOGF("    Forward path: %s\nReturn Path: %s", im.forward_path().c_str(), im.return_path().c_str());
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();

	// allocate session state and data queue for this session
	session::SessionInfo* info = new session::SessionInfo();
	ctx_to_session_info[ctx] = info;

	// parse paths into session state msg
	vector<string> forwardNames = split(im.forward_path(), ',');
	vector<string> returnNames = split(im.return_path(), ',');
	if (forwardNames.size() <= 0) {
		ERROR("Forward path is empty");
		rcm->set_message("Forward path is empty");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
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
	
	// set context info status to initialized session
	session::ContextInfo *ctxInfo = ctx_to_ctx_info[ctx];
	ctxInfo->set_type(session::ContextInfo::SESSION);
	ctxInfo->set_my_name(info->my_name());
	ctxInfo->set_initialized(true);
	allocateDataQ(ctx);


	// If necessary, make a socket for the last hop to connect to
	session::ConnectionInfo *rx_cinfo = NULL;
	string prevhop = get_prevhop_name(ctx);
	int lsock = -1; // might not get used

	bool two_party = (session_hop_count(ctx) == 2);
	pthread_mutex_lock(&name_to_conn_mutex);
	//bool connection_exists = !( name_to_conn.find(prevhop) == name_to_conn.end() && session_hop_count(ctx) > 2 );
	bool connection_exists = !( name_to_conn.find(prevhop) == name_to_conn.end() );

	if (!connection_exists && !two_party) {  // NOT ALREADY A CONNECTION
LOGF("Ctx %d    No connection exists, making rx_sock", ctx);
		// make a socket to accept connection from last hop
		string *addr_buf = NULL;
		lsock = bindRandomAddr(&addr_buf);
		if (lsock < 0) {
			ERROR("Error binding to random address");
			rcm->set_message("Error binding to random address");
			rcm->set_rc(session::FAILURE);
			pthread_mutex_unlock(&name_to_conn_mutex);
			return -1;
		}
		
		// store this rx-transport-conn-to-be so others connecting to the same
		// lasthop don't make their own rx conns (actual sock filled in below)
		rx_cinfo = new session::ConnectionInfo();
		rx_cinfo->set_name(prevhop);
		rx_cinfo->set_my_name(info->my_name());
		rx_cinfo->add_sessions(ctx);

		name_to_conn[prevhop] = rx_cinfo;
		pthread_mutex_unlock(&name_to_conn_mutex);
	
		// store addr in session info
		info->set_my_addr(*addr_buf);
		info->set_initiator_addr(*addr_buf);
		delete addr_buf;
LOG("    Made socket to accept synack from last hop");
	
	} else {
LOGF("Ctx %d    Found an exising connection, making acceptQ", ctx);
		// allocate an acceptQ for this context (to be dealloc'd once we
		// get the synack)
		allocateAcceptQ(ctx);

		// tell the last hop our context # so things get linked up correctly
		// when we get a synack on this end
		info->set_initiator_ctx(ctx);

		// add this session to the connection (do this now to protect against
		// the connection being closed while we send around the session info)
		if (!two_party) {
			rx_cinfo = name_to_conn[prevhop];
			rx_cinfo->add_sessions(ctx);
		}
		pthread_mutex_unlock(&name_to_conn_mutex);
	}

	// send session info to next hop
	session::SessionPacket pkt;
	pkt.set_type(session::SETUP); 
	pkt.set_sender_ctx(ctx);
	session::SessionInfo *newinfo = pkt.mutable_info();// TODO: better way?
	infocpy(info, newinfo);
	if (send(ctx, &pkt) < 0) {
		ERROR("Error sending session info to next hop");
		rcm->set_message("Error sending session info to next hop");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
LOGF("Ctx %d    Sent connection request to next hop", ctx);


	if (two_party) {
		rx_cinfo = name_to_conn[prevhop];
		rx_cinfo->add_sessions(ctx);
	}


	session::SessionPacket *rpkt = new session::SessionPacket();
	int rxsock = -1;
	if ( !connection_exists && !two_party ) {  // NOT ALREADY A CONNECTION
		// accept connection on listen sock
		if ( (rxsock = acceptSock(lsock)) < 0 ) {
			ERRORF("Error accepting connection from last hop on context %d", ctx);
			rcm->set_message("Error accepting connection from last hop");
			rcm->set_rc(session::FAILURE);
			return -1;
		}
		closeSock(lsock);
LOGF("Ctx %d    Accepted a new connection on rxsock %d", ctx, rxsock);

		// fill in the connection info we didn't have above
		rx_cinfo->set_sockfd(rxsock);


		// listen for "synack" -- receive and check incoming session info msg
		if ( recv(rx_cinfo, rpkt) < 0) {
			ERROR("Error receiving synack session info msg");
			rcm->set_message("Error receiving synack session info msg");
			rcm->set_rc(session::FAILURE);
			return -1;
		} 

		// start a thread reading this socket
		int rc;
		if ( (rc = startPollingSocket(rx_cinfo->sockfd(), poll_recv_sock, (void*)rx_cinfo) < 0) ) {
			ERRORF("Error creating recv sock thread: %s", strerror(rc));
			return -1;
		}
	} else {
		// get synack session info from accept Q
		rpkt = popAcceptQ(ctx);
		deallocateAcceptQ(ctx);

		if (rpkt == NULL) {
			ERROR("Trying to listen on a closed context");
			rcm->set_message("Trying to listen on a closed context");
			rcm->set_rc(session::FAILURE);
			return -1;
		}
		
		rxsock = rx_cinfo->sockfd();
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
LOGF("Ctx %d    Got final synack from: %s (sender ctx: %d)", ctx, sender_name.c_str(), rpkt->sender_ctx());
	
	// store incoming ctx mapping
	setLocalContextForIncomingContext(ctx, rpkt->sender_ctx(), rxsock);

	// TODO: set session state to ESTABLISHED
	
	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_bind_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_bind_msg");
	const session::SBindMsg bm = msg.s_bind();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
LOGF("    Binding to name: %s", bm.name().c_str());

	// bind to random app ID
	string *addr_buf = NULL;
	int sock = bindRandomAddr(&addr_buf);
	if (sock < 0) {
		ERROR("Error binding to random address");
		rcm->set_message("Error binding to random address");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	

	// store port as this context's listen sock. This context does not have
	// tx or rx sockets
	ctx_to_listensock[ctx] = sock;

LOGF("    Registering name: %s", bm.name().c_str());
	// register name
    if (registerName(bm.name(), addr_buf) < 0) {
    	ERRORF("error registering name: %s\n", bm.name().c_str());
		rcm->set_message("Error registering name");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
LOG("    Registered name");

	// if everything worked, store name and addr in context info
	session::ContextInfo *ctxInfo = new session::ContextInfo();
	ctxInfo->set_type(session::ContextInfo::LISTEN);
	ctxInfo->set_my_name(bm.name());
	ctxInfo->set_my_addr(*addr_buf);
	ctxInfo->set_initialized(true);
	ctx_to_ctx_info[ctx] = ctxInfo;
	
	// store a mapping of name -> listen_ctx
	name_to_listen_ctx[bm.name()] = ctx;

	// initialize a queue to store incoming Connection Requests
	// (these might arrive on the accept socket or on another socket,
	// since the session layer shares transport connections among sessions)
	allocateAcceptQ(ctx);

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
LOG("BEGIN process_accept_msg");
	
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	const session::SAcceptMsg am = msg.s_accept();
	uint32_t new_ctx = am.new_ctx();

	// check that this context is an initialized listen context
	session::ContextInfo *listenCtxInfo = ctx_to_ctx_info[ctx];
	if (!listenCtxInfo->initialized() || listenCtxInfo->type() != session::ContextInfo::LISTEN) {
		ERROR("Context was not an initialized listen context");
		rcm->set_message("Context was not an initialized listen context");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	// pull message from acceptQ
	session::SessionPacket *rpkt = popAcceptQ(ctx);
	if (rpkt == NULL) {
		ERROR("Trying to listen on a closed context");
		rcm->set_message("Trying to listen on a closed context");
		rcm->set_rc(session::FAILURE);
		return -1;
	}
	assert(rpkt);

	// store a copy of the session info, updating my_name and my_addr
	//session::SessionInfo *sinfo = new session::SessionInfo(rpkt->info());
	session::SessionInfo *sinfo = new session::SessionInfo();
	infocpy(&(rpkt->info()), sinfo);
	sinfo->set_my_name(listenCtxInfo->my_name());
	sinfo->clear_my_addr();
	ctx_to_session_info[new_ctx] = sinfo;

	// allocate context info and a data queue for the new session context
	session::ContextInfo *ctxInfo = new session::ContextInfo();
	ctxInfo->set_type(session::ContextInfo::SESSION);
	ctxInfo->set_initialized(true);
	ctxInfo->set_my_name(listenCtxInfo->my_name());
	ctx_to_ctx_info[new_ctx] = ctxInfo;
	allocateDataQ(new_ctx);
	
	// save  rx connection by name for others to use
	string prevhop = get_prevhop_name(new_ctx);
LOGF("    Got conn req from %s on context %d (sender ctx %d)", prevhop.c_str(), new_ctx, rpkt->sender_ctx());
	session::ConnectionInfo *rx_cinfo = name_to_conn[prevhop];
	rx_cinfo->add_sessions(new_ctx);
	ctx_to_rxconn[new_ctx] = rx_cinfo; // TODO: set this in api call

	
	// store incoming ctx mapping
	setLocalContextForIncomingContext(new_ctx, rpkt->sender_ctx(), ctx_to_rxconn[new_ctx]->sockfd());


	// connect to next hop
	session::SessionPacket pkt;
	pkt.set_type(session::SETUP); 
	pkt.set_sender_ctx(new_ctx);
	session::SessionInfo *newinfo = pkt.mutable_info();// TODO: better way?
	infocpy(sinfo, newinfo);
	
	// If i'm the last hop, the initiator doesn't have a name in the name service.
	// So, we need to use the DAG that was supplied by the initiator and build the
	// tx_conn ourselves. (Unless we already have a connection open with them.)
	// BUT: if it's just me and the initiator, we already have a connection.
	if ( is_last_hop(new_ctx, sinfo->my_name()) && session_hop_count(new_ctx) > 2 ) {
LOG("    I'm the last hop; connecting to initiator with supplied addr");
		
		string nexthop = get_nexthop_name(new_ctx);
		session::ConnectionInfo *tx_cinfo = NULL;
		if ( name_to_conn.find(nexthop) == name_to_conn.end() ) { // NO CONNECTION ALREADY
			tx_cinfo = new session::ConnectionInfo();
			tx_cinfo->set_name(nexthop);
		
			if (!sinfo->has_initiator_addr()) {
				ERROR("Initiator did not send address");
				rcm->set_message("Initiator did not send address");
				rcm->set_rc(session::FAILURE);
				return -1;
			}

			if (openConnectionToAddr(&(sinfo->initiator_addr()), tx_cinfo) < 0) {
				ERROR("Error connecting to initiator");
				rcm->set_message("Error connecting to initiator");
				rcm->set_rc(session::FAILURE);
				return -1;
			}

			// start a thread reading this socket
			// TODO: somehow make this part of open_connection_xia
			int ret;
			if ( (ret = startPollingSocket(tx_cinfo->sockfd(), poll_recv_sock, (void*)tx_cinfo) < 0) ) {
				ERRORF("Error creating recv sock thread: %s", strerror(ret));
				return -1;
			}

			name_to_conn[nexthop] = tx_cinfo;
		} else {
			tx_cinfo = name_to_conn[nexthop];
		}

		tx_cinfo->add_sessions(new_ctx);
		ctx_to_txconn[new_ctx] = tx_cinfo;
	} 

		
	// send session info to next hop
	if (send(new_ctx, &pkt) < 0) {
		ERROR("Error connecting to or sending session info to next hop");
		rcm->set_message("Error connecting to or sending session info to next hop");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

LOG("    Sent SessionInfo packet to next hop");

	delete rpkt; // TODO: not freed if we hit an error and returned early

	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_send_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
//LOG("BEGIN process_send_msg");

	uint32_t sent = 0;
	const session::SSendMsg sendm = msg.s_send();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();

	// Check that ctx is an initialized session context
	if (!checkContext(ctx, session::ContextInfo::SESSION)) {
		ERROR("Context was not an initialized session context");
		rcm->set_message("Context was not an initialized session context");
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
//LOG("BEGIN process_recv_msg");

	const session::SRecvMsg rm = msg.s_recv();
	uint32_t max_bytes = rm.bytes_to_recv();

	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	// Check that ctx is an initialized session context
	if (!checkContext(ctx, session::ContextInfo::SESSION)) {
		ERRORF("Context %d is not an initialized session context", ctx);
		rcm->set_message("Context was not an initialized session context");
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
			ERROR("Trying to receive on a closed session");
			rcm->set_message("Trying to receive on a closed session");
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
	while ( (data.size() < max_bytes && !waitForADU && ctx_to_dataQ[ctx]->size() > 0) 
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
LOG("BEGIN process_close_msg");
	
	const session::SCloseMsg cm = msg.s_close();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();

	closeSession(ctx);
	
	// return success
	rcm->set_rc(session::SUCCESS);
	return 1;
}

int process_check_for_data_msg(int ctx, session::SessionMsg &msg, session::SessionMsg &reply) {
LOG("BEGIN process_check_for_data_msg");
	
	const session::SCloseMsg cm = msg.s_close();
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	
	// Check that ctx is an initialized session context
	if (!checkContext(ctx, session::ContextInfo::SESSION)) {
		ERROR("Context was not an initialized session context");
		rcm->set_message("Context was not an initialized session context");
		rcm->set_rc(session::FAILURE);
		return -1;
	}

	session::SCheckDataRet *cdret = reply.mutable_s_check_data_ret();

	pthread_mutex_lock(ctx_to_dataQ_mutex[ctx]);
	cdret->set_data_available( (ctx_to_dataQ[ctx]->size() > 0) ); 
	pthread_mutex_unlock(ctx_to_dataQ_mutex[ctx]);


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
	LOGF("Listening on %d", procport);
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
	LOGF("Click sockconf name: %s", sockconf_name.c_str());
#endif /* XIA */
			
	// Start a thread watching for mobility
	int pthread_rc;
	if ( (pthread_rc = pthread_create(&mobility_thread, NULL, mobility_daemon, NULL)) != 0) {
		ERRORF("Error creating thread: %s", strerror(pthread_rc));
	}

	// Initializes some mutexes
	pthread_mutex_init(&name_to_conn_mutex, NULL);
	pthread_mutex_init(&incomingctx_to_myctx_mutex, NULL);

	listen();
	return 0;
}
