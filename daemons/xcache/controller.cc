#include <fcntl.h>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include "controller.h"
#include <iostream>
#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include "xcache_sock.h"
#include "cid.h"

#define IGNORE_PARAM(__param) ((void)__param)
#define IO_BUF_SIZE (1024 * 1024)
#define MAX_XID_SIZE 100

enum {
	RET_FAILED = -1,
	RET_OK,
	RET_SENDRESP,
	RET_NORESP,
	RET_REMOVEFD,
	RET_ENQUEUE,
};

static int send_response(int fd, const char *buf, size_t len)
{
	int ret = 0;
	uint32_t msg_length = htonl(len);

	ret = send(fd, &msg_length, 4, 0);
	if(ret <= 0)
		return ret;

	ret = send(fd, buf, len, 0);
	return ret;
}

static int xcache_create_lib_socket(void)
{
	struct sockaddr_un my_addr;
	int s;
	char socket_name[512];

	s = socket(AF_UNIX, SOCK_STREAM, 0);

	my_addr.sun_family = AF_UNIX;
	if(get_xcache_sock_name(socket_name, 512) < 0) {
		return -1;
	}

	remove(socket_name);

	strcpy(my_addr.sun_path, socket_name);

	bind(s, (struct sockaddr *)&my_addr, sizeof(my_addr));

	// FIXME: ! this is a bit of a security issue as it's making the xcache
	// socket open to everyone. The alternate is to run client side apps as
	// root which is a pain. It might be better to just change things to use
	// normal sockets on localhost.
	chmod(socket_name, 0x777);

	listen(s, 100);

	return s;
}

// FIXME: unused?
void xcache_controller::status(void)
{
	syslog(LOG_INFO, "[Status]");
}

int xcache_controller::fetch_content_remote(sockaddr_x *addr, socklen_t addrlen,
					    xcache_cmd *resp, xcache_cmd *cmd,
					    int flags)
{
	int to_recv, recvd, sock;
	Graph g(addr);

	syslog(LOG_INFO, "Fetching content from remote DAG = %s\n", g.dag_string().c_str());

	sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if(sock < 0) {
		syslog(LOG_ERR, "Unable to create socket: %s", strerror(errno));
		return RET_FAILED;
	}

	if (Xconnect(sock, (struct sockaddr *)addr, addrlen) < 0) {
		syslog(LOG_ERR, "connect failed: %s\n", strerror(errno));
		Xclose(sock);
		return RET_FAILED;
	}

	syslog(LOG_INFO, "Xcache client now connected with remote");

	std::string data;
	char buf[IO_BUF_SIZE];
	struct cid_header header;
	size_t remaining;
	size_t offset;
	unsigned hop_count;

	Node expected_cid(g.get_final_intent());
	xcache_meta *meta = new xcache_meta(expected_cid.id_string());

	meta->set_state(FETCHING);

	remaining = sizeof(cid_header);
	offset = 0;

	while (remaining > 0) {
		syslog(LOG_DEBUG, "Remaining(1) = %lu\n", remaining);
		recvd = Xrecv(sock, (char *)&header + offset, remaining, 0);
		if (recvd < 0) {
			syslog(LOG_ALERT, "Sender Closed the connection: %s", strerror(errno));
			Xclose(sock);
			assert(0);
			break;
		} else if (recvd == 0) {
			syslog(LOG_INFO, "Xrecv returned 0\n");
			break;
		}
		remaining -= recvd;
		offset += recvd;
	}

	remaining = ntohl(header.length);
	hop_count = ntohl(header.hop_count);
	meta->set_created();
	meta->set_ttl(ntohl(header.ttl));

	while (remaining > 0) {
		to_recv = remaining > IO_BUF_SIZE ? IO_BUF_SIZE : remaining;

		recvd = Xrecv(sock, buf, to_recv, 0);
		if (recvd < 0) {
			syslog(LOG_ERR, "Receiver Closed the connection; %s", strerror(errno));
			Xclose(sock);
			assert(0);
			break;
		} else if (recvd == 0) {
			syslog(LOG_WARNING, "Xrecv returned 0");
			break;
		}
		syslog(LOG_DEBUG, "recvd = %d, to_recv = %d\n", recvd, to_recv);

		remaining -= recvd;
		std::string temp(buf, recvd);

		data += temp;
	}

	std::string computed_cid = compute_cid(data.c_str(), data.length());
	struct xcache_context *context = lookup_context(cmd->context_id());

	if (!context) {
		syslog(LOG_WARNING, "Context Lookup Failed");
		delete meta;
		Xclose(sock);
		return RET_FAILED;
	}

	if ((flags & XCF_CACHE)) {
		if (__store(context, meta, (const std::string *)&data) == RET_FAILED) {
			delete meta;
			Xclose(sock);
			return RET_FAILED;
		}
	} else {
		if (verify_content(meta, (const std::string *)&data) == false) {
			delete meta;
			Xclose(sock);
			return RET_FAILED;
		}
	}

	if (resp) {
		resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
		resp->set_data(data);
		resp->set_hop_count(hop_count);
	}

	Xclose(sock);

	return RET_OK;
}

int xcache_controller::fetch_content_local(sockaddr_x *addr, socklen_t addrlen,
					   xcache_cmd *resp, xcache_cmd *cmd,
					   int flags)
{
	xcache_meta *meta;
	std::string data;
	Graph g(addr);
	Node expected_cid(g.get_final_intent());
	std::string cid(expected_cid.id_string());

	IGNORE_PARAM(flags);
	IGNORE_PARAM(addrlen);
	IGNORE_PARAM(cmd);
	syslog(LOG_INFO, "Fetching content %s from local\n", expected_cid.id_string().c_str());

	try {
		meta = new xcache_meta(cid);
		data = store_manager.get(meta);
	} catch(int e) {
        // We could not find the content locally
		syslog(LOG_WARNING, "meta not found");
		return RET_FAILED;
	}

	if(resp) {
		resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
		resp->set_data(data);
		resp->set_hop_count(0);
		resp->set_ttl(meta->ttl());
	}

	syslog(LOG_INFO, "Fetching content from local DONE\n");
	return RET_OK;

}

int xcache_controller::xcache_notify(struct xcache_context *c, sockaddr_x *addr,
				     socklen_t addrlen, int event)
{
	xcache_notif notif;
	std::string buffer("");

	notif.set_cmd(event);
	notif.set_dag(addr, addrlen);
	notif.SerializeToString(&buffer);

	syslog(LOG_INFO, "Sent Notification to %d\n", c->notifSock);

	return send_response(c->notifSock, buffer.c_str(), buffer.length());
}

struct __fetch_content_args {
	xcache_controller *ctrl;
	xcache_cmd *resp;
	xcache_cmd *cmd;
	int ret;
	int flags;
};

void *xcache_controller::__fetch_content(void *__args)
{
	int ret;
	struct __fetch_content_args *args = (struct __fetch_content_args *)__args;
	sockaddr_x addr;
	size_t daglen;

	// FIXME: should client specify a ttl? or hornor the one sent by the server
	// or should we not cache locally, or should eviction be manual only on the client?

	daglen = args->cmd->dag().length();
	memcpy(&addr, args->cmd->dag().c_str(), args->cmd->dag().length());

	ret = args->ctrl->fetch_content_local(&addr, daglen, args->resp,
					      args->cmd, args->flags);

	if(ret == RET_FAILED) {
		ret = args->ctrl->fetch_content_remote(&addr, daglen, args->resp,
						       args->cmd, args->flags);
		if (ret == RET_FAILED) {
			syslog(LOG_ERR, "Remote content fetch failed: %d", ret);
			args->resp->set_cmd(xcache_cmd::XCACHE_ERROR);
		}
	}

	args->ret = ret;

	if (!(args->flags & XCF_BLOCK)) {
		/*
		 * FIXME: In case of error must notify the error
		 */
		struct xcache_context *c = args->ctrl->lookup_context(args->cmd->context_id());

		if (c)
			args->ctrl->xcache_notify(c, &addr, daglen, XCE_CHUNKARRIVED);

		// FIXME: mixing allocation methods is troublesome and prone to weird errors
		//   when you delete instead of free something that was malloc'd
		delete args->cmd;
		free(args);
	}

	return NULL;
}

int xcache_controller::xcache_fetch_content(xcache_cmd *resp, xcache_cmd *cmd,
					    int flags)
{
	int ret = 0;

	if (flags & XCF_BLOCK) {
		struct __fetch_content_args args;

		args.ctrl = this;
		args.resp = resp;
		args.cmd = cmd;
		args.ret = ret;
		args.flags = flags;
		__fetch_content(&args);
		ret = RET_SENDRESP;
	} else {
		/* FIXME: Add to worker queue */
		struct __fetch_content_args *args =
			(struct __fetch_content_args *)
			malloc(sizeof(struct __fetch_content_args));

		pthread_t fetch_thread;
		args->ctrl = this;
		args->ret = ret;
		args->flags = flags;
		args->resp = NULL;
		args->cmd = new xcache_cmd(*cmd);
		pthread_create(&fetch_thread, NULL, __fetch_content, args);
		ret = RET_NORESP;
	}

	return ret;
}

std::string xcache_controller::addr2cid(sockaddr_x *addr)
{
	Graph g(addr);
	Node cid_node(g.get_final_intent());
	return cid_node.id_string();
}


int xcache_controller::chunk_read(xcache_cmd *resp, xcache_cmd *cmd)
{
	xcache_meta *meta = acquire_meta(addr2cid((sockaddr_x *)(cmd->dag().c_str())));
	std::string data;

	if(!meta) {
		resp->set_cmd(xcache_cmd::XCACHE_ERROR);
		resp->set_msg("Invalid address\n");
		return RET_SENDRESP;
	}

	data = meta->get(cmd->readoffset(), cmd->readlen());
	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_data(data);

	release_meta(meta);
	return RET_SENDRESP;
}

int xcache_controller::fast_process_req(int fd, xcache_cmd *resp, xcache_cmd *cmd)
{
	int ret = RET_NORESP;
	xcache_cmd response;
	struct xcache_context *c;

	switch(cmd->cmd()) {
	case xcache_cmd::XCACHE_ALLOC_CONTEXT:
		syslog(LOG_INFO, "Received ALLOC_CONTEXTID\n");
		ret = alloc_context(resp, cmd);
		break;

	case xcache_cmd::XCACHE_FREE_CONTEXT:
		syslog(LOG_INFO, "Freeing context_id %u\n", cmd->context_id());
		ret = free_context(cmd);
		break;

	case xcache_cmd::XCACHE_FLAG_DATASOCKET:
		c = lookup_context(cmd->context_id());

		if (!c)
			syslog(LOG_ALERT, "Should Not Happen");
		else
			c->xcacheSock = fd;

		break;

	case xcache_cmd::XCACHE_FLAG_NOTIFSOCK:
		c = lookup_context(cmd->context_id());
		if(!c) {
			syslog(LOG_ALERT, "Should Not Happen");
		} else {
			c->notifSock = fd;
			ret = RET_REMOVEFD;
		}
		break;

	case xcache_cmd::XCACHE_EVICT:
	case xcache_cmd::XCACHE_STORE:
	case xcache_cmd::XCACHE_FETCHCHUNK:
	case xcache_cmd::XCACHE_READ:
		ret = RET_ENQUEUE;
		break;

	default:
		syslog(LOG_WARNING, "Unknown message received\n");
	}

	return ret;
}


struct xcache_context *xcache_controller::lookup_context(int context_id)
{
	std::map<uint32_t, struct xcache_context *>::iterator i = context_map.find(context_id);

	if(i != context_map.end()) {
		return i->second;
	} else {
		syslog(LOG_WARNING, "Context %d NOT found.\n", context_id);
		return NULL;
	}
}

int xcache_controller::free_context(xcache_cmd *cmd)
{
	unsigned id = cmd->context_id();
	xcache_context *c = context_map[id];

	if (c) {
		close(c->notifSock);
		close(c->xcacheSock);

		context_map.erase(id);
		free(c);
	}

	return RET_NORESP;
}

int xcache_controller::alloc_context(xcache_cmd *resp, xcache_cmd *cmd)
{
	struct xcache_context *c = (struct xcache_context *)malloc(sizeof(struct xcache_context));
	IGNORE_PARAM(cmd);

	// FIXME: race condition!
	++context_id;

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_context_id(context_id);

	memset(c, 0, sizeof(struct xcache_context));
	c->contextID = context_id;
	context_map[context_id] = c;
	syslog(LOG_INFO, "Allocated Context %d\n", resp->context_id());

	return RET_SENDRESP;
}

int xcache_controller::__store_policy(xcache_meta *meta) {
	return policy_manager.cacheable(meta);
}

bool xcache_controller::verify_content(xcache_meta *meta, const std::string *data)
{
	if (meta->get_cid() != compute_cid(data->c_str(), data->length()))
		return false;

	return true;
}

int xcache_controller::__store(struct xcache_context * /*context */,
			       xcache_meta *meta, const std::string *data)
{
	meta->lock();

	syslog(LOG_DEBUG, "[thread %lu] after locking meta map and meta\n", pthread_self());

	if (verify_content(meta, data) == false) {
		// Content Verification Failed
		syslog(LOG_ERR, "Content Verification Failed");
		meta->unlock();
		return RET_FAILED;
	}

	meta->set_length(data->size());	// size of the data in bytes
	if (__store_policy(meta) < 0) {
		syslog(LOG_ERR, "Context store failed\n");
		meta->unlock();
		return RET_FAILED;
	}
	syslog(LOG_DEBUG, "[thread %lu] after store policy\n", pthread_self());

	std::string cid = "CID:" + meta->get_cid();

	store_manager.store(meta, data);

	meta->set_state(AVAILABLE);

	_map.add_meta(meta);

	meta->unlock();

	register_meta(cid);

	return RET_SENDRESP;
}

int xcache_controller::cid2addr(std::string cid, sockaddr_x *sax)
{
	int rc = 0;
	std::string myCid("CID:");

	struct addrinfo *ai;

	myCid += cid;

	// make a direct AD-HID-SID dag for the xcache daemon
	if (Xgetaddrinfo(NULL, xcache_sid.c_str(), NULL, &ai) >= 0) {
		if (ai->ai_addr) {
			// now append the CID to it
			Node cnode(myCid);
			Graph spath((sockaddr_x*)ai->ai_addr);
			spath *= cnode;

			// now make a direct ->CID path
			Graph cpath = Node() * cnode;

			// make final dag
			// * => CID is primary
			// * => AD => HID => SID => CID is fallback
			Graph dag = cpath + spath;

			sockaddr_x xaddr;
			dag.fill_sockaddr(&xaddr);
			memcpy(sax, &xaddr, sizeof(sockaddr_x));
		} else {
			rc = -1;
		}

		Xfreeaddrinfo(ai);
	} else {
		rc = -1;
	}

	return rc;
}





int xcache_controller::evict(xcache_cmd *resp, xcache_cmd *cmd)
{
	std::string cid = cmd->cid();

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);

	// cid was vaidated by in the API call
	syslog(LOG_INFO, "evict called for cid:%s!\n", cmd->cid().c_str());

	xcache_meta *meta = acquire_meta(cid.c_str());

	if (meta) {
		std::string c = "CID:" + cid;

		switch (meta->state()) {
			case AVAILABLE:
				xr.delRoute(c);
				// let the garbage collector do the actual data removal
				meta->set_state(EVICTING);

			case EVICTING:
				// we're in process of evicting...
				resp->set_status(xcache_cmd::XCACHE_OK);
				break;

			case FETCHING:
				xr.delRoute(c);
				// mark the chunk to be evicted once it's out of fetching state
				resp->set_status(xcache_cmd::XCACHE_CID_MARKED_FOR_DELETE);
				break;

			case CACHING:

				resp->set_status(xcache_cmd::XCACHE_CID_IN_USE);
				break;

			default:
				resp->set_status(xcache_cmd::XCACHE_CID_NOT_FOUND);
				break;
		}

		release_meta(meta);

	} else {
		resp->set_status(xcache_cmd::XCACHE_CID_NOT_FOUND);
	}

	return RET_SENDRESP;
}

int xcache_controller::store(xcache_cmd *resp, xcache_cmd *cmd, time_t ttl)
{
	struct xcache_context *context;
	xcache_meta *meta;
	std::string cid = compute_cid(cmd->data().c_str(), cmd->data().length());

	meta = acquire_meta(cid.c_str());

	if (meta != NULL) {
		/*
		 * Meta already exists
		 */
		release_meta(meta);
		resp->set_status(xcache_cmd::XCACHE_ERR_EXISTS);
	} else {

        // FIXME: fast implementation here, store meta to stored (mongo) as well
		/*
		 * New object - Allocate a meta
		 */
		meta = new xcache_meta(cid);
		meta->set_ttl(ttl);
		meta->set_created();
		meta->set_length(cmd->data().length());
printf("store:length: %lu", meta->get_length());
		context = lookup_context(cmd->context_id());
		if(!context) {
			return RET_FAILED;
		}

		if(__store(context, meta, &cmd->data()) == RET_FAILED) {
			return RET_FAILED;
		}

	}

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);

	sockaddr_x addr;

	if (cid2addr(cid, &addr) == 0) {
		resp->set_dag((char *)&addr, sizeof(sockaddr_x));
		Graph g(&addr);
		syslog(LOG_INFO, "store: %s", g.dag_string().c_str());
	} else {
		// FIXME: do some sort of error handling here
	}

	syslog(LOG_INFO, "Store Finished\n");

	return RET_SENDRESP;
}

// FIXME:this should set fetching state so we can't delete the data/route
// out from under us
void xcache_controller::send_content_remote(xcache_req* req, sockaddr_x *mypath)
{
	Graph g(mypath);
	Node cid(g.get_final_intent());
	xcache_cmd resp;

	syslog(LOG_INFO, "CID = %s\n", cid.id_string().c_str());

	if (fetch_content_local(mypath, sizeof(sockaddr_x), &resp, NULL, 0) != RET_OK) {
		syslog(LOG_ALERT, "This should not happen");
		assert(0);
	}

	struct cid_header header;
	size_t remaining;
	size_t offset;
	int sent;

	header.version = htons(CID_HEADER_VER);
	header.hlen = htons(sizeof(struct cid_header));
	header.length = htonl(resp.data().length());
	header.hop_count = htonl(req->hop_count);
	header.ttl = htonl(resp.ttl());

	remaining = sizeof(header);
	offset = 0;

	syslog(LOG_DEBUG, "Header Send Start\n");

	while (remaining > 0) {
		sent = Xsend(req->to_sock, (char *)&header + offset, remaining, 0);
		if (sent < 0) {
			syslog(LOG_ALERT, "Receiver Closed the connection %s", strerror(errno));
			assert(0);
			Xclose(req->to_sock);
		}
		remaining -= sent;
		offset += sent;
	}

	remaining = resp.data().length();
	offset = 0;

	syslog(LOG_DEBUG, "Content Send Start\n");

	while (remaining > 0) {
		sent = Xsend(req->to_sock, (char *)resp.data().c_str() + offset,
			     remaining, 0);
		syslog(LOG_DEBUG, "Sent = %d\n", sent);
		if (sent < 0) {
			syslog(LOG_ALERT, "Receiver Closed the connection: %s", strerror(errno));
			assert(0);
			Xclose(req->to_sock);
		}
		remaining -= sent;
		offset += sent;
		syslog(LOG_DEBUG, "Content Send Remaining %lu\n", remaining);
	}
	syslog(LOG_DEBUG, "Send Done\n");

}

int xcache_controller::create_sender(void)
{
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	int xcache_sock;
	struct addrinfo *ai;

	if ((xcache_sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		return -1;

	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		syslog(LOG_ERR, "XmakeNewSID failed\n");
		return -1;
	}

	if (XsetXcacheSID(xcache_sock, sid_string, strlen(sid_string)) < 0) {
		syslog(LOG_ERR, "XsetXcacheSID failed\n");
		return -1;
	}

	// save our SID in the class struct for later use
	xcache_sid = sid_string;

	syslog(LOG_INFO, "XcacheSID is %s\n", sid_string);

	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0)
		return -1;

	sockaddr_x *dag = (sockaddr_x *)ai->ai_addr;

	if (Xbind(xcache_sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(xcache_sock);
		Xfreeaddrinfo(ai);
		return -1;
	}

	Xlisten(xcache_sock, 5);

	Graph g(dag);
	syslog(LOG_INFO, "Listening on dag: %s\n", g.dag_string().c_str());

	Xfreeaddrinfo(ai);
	return xcache_sock;
}

int xcache_controller::register_meta(std::string &cid)
{
	int rv;
	std::string empty_str("");

	syslog(LOG_DEBUG, "[thread %lu] Setting Route for %s.\n",  pthread_self(), cid.c_str());
	rv = xr.setRoute(cid, DESTINED_FOR_LOCALHOST, empty_str, 0);
	syslog(LOG_DEBUG, "[thread %lu] status code %d error message %s\n", pthread_self(), rv, xr.cserror());

	return rv;
}

void xcache_controller::enqueue_request_safe(xcache_req *req)
{
	pthread_mutex_lock(&request_queue_lock);
	request_queue.push(req);
	pthread_mutex_unlock(&request_queue_lock);

	sem_post(&req_sem);
}

xcache_req *xcache_controller::dequeue_request_safe(void)
{
	xcache_req *ret;

	sem_wait(&req_sem);

	pthread_mutex_lock(&request_queue_lock);
	ret = request_queue.front();
	request_queue.pop();
	pthread_mutex_unlock(&request_queue_lock);

	return ret;
}

void xcache_controller::process_req(xcache_req *req)
{
	xcache_cmd resp, *cmd;
	int ret;
	std::string buffer;

	ret = RET_NORESP;

	switch(req->type) {
	case xcache_cmd::XCACHE_CACHE:
	{
		syslog(LOG_INFO, "XCACHE_CACHE");
		xcache_meta *meta = acquire_meta(req->cid);
		if (meta) {
			std::string chunk((const char *)req->data, req->datalen);
			release_meta(meta);
			__store(NULL, meta, &chunk);
			free(req->cid);
		}
		break;
	}

	case xcache_cmd::XCACHE_STORE:
		syslog(LOG_INFO, "XCACHE_STORE");
		ret = store(&resp, (xcache_cmd *)req->data, req->ttl);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_FETCHCHUNK:
		syslog(LOG_INFO, "XCACHE_FETCHCHUNK");
		cmd = (xcache_cmd *)req->data;
		ret = xcache_fetch_content(&resp, cmd, cmd->flags());
		syslog(LOG_INFO, "xcache_fetch_content returned %d", ret);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_READ:
		syslog(LOG_INFO, "XCACHE_READ");
		cmd = (xcache_cmd *)req->data;
		ret = chunk_read(&resp, cmd);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_SENDCHUNK:
		syslog(LOG_INFO, "XCACHE_SENDCHUNK");
		send_content_remote(req, (sockaddr_x *)req->data);
		break;

	case xcache_cmd::XCACHE_EVICT:
		syslog(LOG_INFO, "XCACHE_EVICT");
		cmd = (xcache_cmd *)req->data;
		ret = evict(&resp, cmd);
		if(ret >= 0) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	}


	if (req->type == xcache_cmd::XCACHE_SENDCHUNK || req->type == xcache_cmd::XCACHE_CACHE) {
		if (req->data) {
			free(req->data);
		}
	} else {
		if (req->data) {
			delete (xcache_cmd*)req->data;
		}
	}

	if(req->flags & XCFI_REMOVEFD) {
		syslog(LOG_INFO, "Closing Socket");
		Xclose(req->to_sock);
	}
	delete req;
}

/**
 * Worker thread
 * This thea
 */
void *xcache_controller::worker_thread(void *arg)
{
	xcache_controller *ctrl = (xcache_controller *)arg;

	syslog(LOG_INFO, "Worker started working");

	while(1) {
		xcache_req *req;

		req = ctrl->dequeue_request_safe();
		if(req) {
			ctrl->process_req(req);
		}
	}

	return NULL;
}

// delete any chunks that are not complete but are too stale
void *xcache_controller::garbage_collector(void *arg)
{
	xcache_controller *ctrl = (xcache_controller *)arg;

	while(1) {
		sleep(GC_INTERVAL);

		meta_map *map = ctrl->get_meta_map();
		map->walk();
	}

    return NULL;
}

void xcache_controller::run(void)
{
	fd_set fds, allfds;
	int max, libsocket, rc, sendersocket;
	struct cache_args args;
	pthread_t gc_thread;

	std::vector<int> active_conns;
	std::vector<int>::iterator iter;

	/* Application interface */
	libsocket = xcache_create_lib_socket();

	/* Arguments for the cache thread */
	args.cache = &cache;
	args.ctrl = this;

	/*
	 * Caching is performed by an independent thread
	 * Please see cache.cc for details
	 */
	cache.spawn_thread(&args);

	if((rc = xr.connect()) != 0) {
		syslog(LOG_ERR, "Unable to connect to click %d \n", rc);
		return;
	}

	xr.setRouter(hostname);
	sendersocket = create_sender();

	FD_ZERO(&allfds);
	FD_SET(libsocket, &allfds);
	FD_SET(sendersocket, &allfds);

	/* Launch all the worker threads */
	for(int i = 0; i < n_threads; i++) {
		pthread_create(&threads[i], NULL, worker_thread, (void *)this);
	}

	// start the garbage collector
	pthread_create(&gc_thread, NULL, garbage_collector, (void *)this);

	syslog(LOG_INFO, "Entering The Loop\n");

repeat:
	memcpy(&fds, &allfds, sizeof(fd_set));

	max = MAX(libsocket, sendersocket);

	/* FIXME: do something better */
	for(iter = active_conns.begin(); iter != active_conns.end(); ++iter) {
		max = MAX(max, *iter);
	}

	rc = Xselect(max + 1, &fds, NULL, NULL, NULL);

	if(FD_ISSET(libsocket, &fds)) {
		int new_connection = accept(libsocket, NULL, 0);

		if (new_connection > 0) {

			active_conns.push_back(new_connection);
			FD_SET(new_connection, &allfds);
		}
	}

	if(FD_ISSET(sendersocket, &fds)) {
		int accept_sock;
		sockaddr_x mypath;
		socklen_t mypath_len = sizeof(mypath);

		syslog(LOG_INFO, "Accepting!");
		if((accept_sock = XacceptAs(sendersocket, (struct sockaddr *)&mypath,
									&mypath_len, NULL, NULL)) < 0) {
			syslog(LOG_ERR, "XacceptAs failed");
		} else {
			// FIXME: reply back the hop count
			unsigned hop_count = XgetPrevAcceptHopCount();
			xcache_req *req = new xcache_req();

			syslog(LOG_INFO, "XacceptAs Succeeded\n");
			Graph g(&mypath);
			g.print_graph();

			/* Create a request to add to worker queue */

			/* Send chunk Request */
			req->type = xcache_cmd::XCACHE_SENDCHUNK;
			req->from_sock = sendersocket;
			req->to_sock = accept_sock;
			req->data = malloc(mypath_len);
			req->hop_count = hop_count;
			memcpy(req->data, &mypath, mypath_len);
			req->datalen = mypath_len;
			/* Indicates that after sending, remove the fd */
			req->flags = XCFI_REMOVEFD;

			enqueue_request_safe(req);
			syslog(LOG_INFO, "Request Enqueued\n");
		}
	}

	for(iter = active_conns.begin(); iter != active_conns.end(); ) {
		if(!FD_ISSET(*iter, &fds)) {
			++iter;
			continue;
		}

		char buf[XIA_MAXBUF];
		std::string buffer("");
		xcache_cmd resp, cmd;
		int ret;
		uint32_t msg_length, remaining;
		bool parse_success = false;

		ret = recv(*iter, &msg_length, sizeof(msg_length), 0);
		if(ret <= 0)
			goto disconnected;

		remaining = ntohl(msg_length);

		if (remaining == 0)
			goto next;

		while(remaining > 0) {
			ret = recv(*iter, buf, MIN(sizeof(buf), remaining), 0);
			syslog(LOG_DEBUG, "Recv returned %d, remaining = %d\n", ret, remaining);
			if(ret <= 0)
				goto disconnected;

			buffer.append(buf, ret);
			remaining -= ret;
		}

		parse_success = cmd.ParseFromString(buffer);

		syslog(LOG_INFO, "Controller received %lu bytes\n", buffer.length());
		if(!parse_success) {
			syslog(LOG_ERR, "Protobuf could not parse\n");
			goto next;
		}

		/*
		 * Try to see if we can process request in the fast path
		 */
		ret = fast_process_req(*iter, &resp, &cmd);

		if(ret == RET_SENDRESP) {
			/*
			 * Send response back to the client
			 */
			resp.SerializeToString(&buffer);
			if(send_response(*iter, buffer.c_str(), buffer.length()) < 0) {
				syslog(LOG_ERR, "FIXME: handle failed return from write\n");
			} else {
				syslog(LOG_INFO, "Data sent to client\n");
			}
		} else if(ret == RET_REMOVEFD) {
			/* Remove this fd from the pool */
			goto removefd;
		} else if(ret == RET_ENQUEUE) {
			/*
			 * Fast path processing not possible:
			 * Enqueue the request into worker queue
			 */
			xcache_req *req = new xcache_req();

			req->type = cmd.cmd();
			req->ttl = cmd.ttl();
			req->from_sock = *iter;
			req->to_sock = *iter;
			req->data = new xcache_cmd(cmd);
			req->datalen = sizeof(xcache_cmd);

			enqueue_request_safe(req);
		}

next:
		++iter;
		continue;

disconnected:
		// FIXME: this should clean up the context somehow
		syslog(LOG_INFO, "Client %d disconnected.\n", *iter);
		close(*iter);
removefd:
		FD_CLR(*iter, &allfds);
		active_conns.erase(iter);
	}

	goto repeat;
}

void xcache_controller::set_conf(struct xcache_conf *conf)
{
	hostname = std::string(conf->hostname);

	n_threads = conf->threads;
	threads = (pthread_t *)malloc(sizeof(pthread_t) * conf->threads);
}
