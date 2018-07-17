#include <fcntl.h>
#include <string>
#include <atomic>
#include <memory>
#include <algorithm>
#include <functional>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
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
#include "Xsecurity.h"
#include "dagaddr.hpp"
#include "headers/ncid_header.h"
#include "ncid_table.h"
#include "xcache_sock.h"
#include "cid.h"

#define IGNORE_PARAM(__param) ((void)__param)
#define IO_BUF_SIZE (1024 * 1024)
#define MAX_XID_SIZE 100
#define MAX_PUSH_PROXIES 10

// The only xcache_controller. Initialized by first call to
// xcache_controller::get_instance()
xcache_controller *xcache_controller::_instance = 0;

xcache_controller::xcache_controller()
{
	_instance = 0;
	_map = meta_map::get_map();
	_ncid_table = NCIDTable::get_table();
	hostname.assign("host0");
	pthread_mutex_init(&request_queue_lock, NULL);
	pthread_mutex_init(&ncid_cid_lock, NULL);
	context_id = 0;
	sem_init(&req_sem, 0, 0);
	_proxy_id = 1;
}

xcache_controller *xcache_controller::get_instance()
{
	if (_instance == 0) {
		_instance = new xcache_controller;
	}
	return _instance;
}

xcache_controller::~xcache_controller()
{
	delete _instance;
	_instance = 0;
}

enum {
	RET_FAILED = -1,
	RET_OK,
	RET_SENDRESP,
	RET_NORESP,
	RET_REMOVEFD,
	RET_ENQUEUE,
	RET_DISCONNECT,
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
					    xcache_cmd *resp,
					    int flags)
{
	int state = 0;	// Cleanup state
	int retval = RET_FAILED;	// Return failure by default

	int sock;
	std::string data;
	std::unique_ptr<Node> expected_cid;
	std::unique_ptr<ContentHeader> chdr;
	std::unique_ptr<xcache_meta> meta;
	std::atomic<bool> stop(false);
	std::string id;
	std::string store_id;

	sockaddr_x src_addr;
	socklen_t src_len;

	// We can check to see if the content header matches sender DAG
	// but due to intrinsic security of CID/NCID, this check is
	// unnecessary. Use expected_cid below if that check is needed.
	Graph g(addr);
	expected_cid.reset(new Node(g.get_final_intent()));
	if(expected_cid == nullptr) {
		syslog(LOG_ERR, "fetch_content_remote: Node allocation failed");
		goto fetch_content_remote_done;
	}
	syslog(LOG_INFO, "Fetching content from remote DAG = %s\n",
			g.dag_string().c_str());

	// Setup a reliable connection to a source of requested CID/NCID
	sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if(sock < 0) {
		syslog(LOG_ERR, "Unable to create socket: %s", strerror(errno));
		return RET_FAILED;
	}
	state = 1;

	if (Xconnect(sock, (struct sockaddr *)addr, addrlen) < 0) {
		syslog(LOG_ERR, "connect failed: %s\n", strerror(errno));
		goto fetch_content_remote_done;
	}
	syslog(LOG_INFO, "Xcache client now connected with remote");

	// reliably receive the entire chuck header and contents
	// Note: this call can be interrupted by setting stop to true
	if(xcache_get_content(sock, data, chdr, stop)) {
		syslog(LOG_INFO, "Unable to fetch remote content");
		goto fetch_content_remote_done;
	}
	assert(chdr != nullptr);

	// Make a copy of chunk contents into "data"
	syslog(LOG_INFO, "Got chunk of size %zu", data.length());

	// New metadata for the CID or NCID
	meta.reset(new xcache_meta());
	if(meta == nullptr) {
		syslog(LOG_ERR, "Unable to create metadata for chunk");
		goto fetch_content_remote_done;
	}
	// find out where we really connected to in case the chunk was cached
	src_len = sizeof(sockaddr_x);
	if (resp != NULL && Xgetpeername(sock, (sockaddr*)&src_addr, &src_len) >= 0) {

		resp->set_source_dag(&src_addr, src_len);

	} else {
		// FIXME: what do we want to do in this case??
	}




	// Store the content header into the metadata
	// Note: chdr will be nullptr after this
	meta->set_content_header(chdr.release());
	meta->set_created();
	meta->set_state(READY_TO_SAVE);
	id = meta->id();
	store_id = meta->store_id();

	if ((flags & XCF_CACHE)) {
		// __store is now going to manage the "meta" object
		// Note: meta will be nullptr after this
		if (__store(meta.release(), data) == RET_FAILED) {
			goto fetch_content_remote_done;
		}
	} else {
		if (meta->valid_data(data) == false) {
			goto fetch_content_remote_done;
		}
	}

	// Map NCID to corresponding CID in NCIDTable
	if(expected_cid->type() == CLICK_XIA_XID_TYPE_NCID) {
		NCIDTable *_ncid_table = NCIDTable::get_table();
		_ncid_table->register_ncid(id, store_id);
	}

	if (resp) {
		resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
		resp->set_data(data);
	}

	// All went well, we now have the chunk
	retval = RET_OK;

fetch_content_remote_done:
	switch(state) {
		case 1: Xclose(sock);
	}
	return retval;
}

/*!
 * @brief Check if the given CID is in local storage
 *
 * Acquire metadata and ensure that the requested content is in local
 * storage. This is useful for services like Fetching Service that
 * can immediately return local content separately from remote fetching.
 *
 * @returns true if the content is local
 * @returns false if the content is remote or unavailable
 */
bool xcache_controller::is_content_local(std::string cid)
{
	xcache_meta *meta = acquire_meta(cid);
	if(meta && meta->state() == AVAILABLE) {
		return true;
	}
	return false;
}

/*!
 * @brief Fetch content from local storage
 *
 * Fetch the requested content from local storage, if available.
 *
 * @param addr DAG of the content to be retrieved
 * @param addrlen length of the content DAG. IGNORED.
 * @param resp response including content retrieved and metadata
 * @param cmd IGNORED
 *
 * @returns RET_FAILED on failure
 * @returns RET_OK on success
 */
int xcache_controller::fetch_content_local(sockaddr_x *addr, socklen_t addrlen,
					   xcache_cmd *resp, xcache_cmd *cmd,
					   int flags)
{
	xcache_meta *meta;
	std::string data;

	// Get the intended CID from given address
	Graph g(addr);

	Node expected_cid(g.get_final_intent());
	syslog(LOG_INFO, "Fetching content %s from local\n",
			expected_cid.to_string().c_str());

	std::string cid = _ncid_table->to_cid(expected_cid.to_string());
	if (cid.size() == 0) {
		syslog(LOG_INFO, "NCID not available locally");
		return RET_FAILED;
	}
	syslog(LOG_INFO, "Reading %s from storage", cid.c_str());

	IGNORE_PARAM(flags);
	IGNORE_PARAM(addrlen);
	IGNORE_PARAM(cmd);

	meta = acquire_meta(cid);
	if(!meta) {
		syslog(LOG_WARNING, "meta not found");
		/* We could not find the content locally */
		return RET_FAILED;
	}

	if(meta->state() != AVAILABLE) {
		release_meta(meta);
		return RET_FAILED;
	}

	syslog(LOG_INFO, "Getting data by calling meta->get()\n");

	// touch the object to move it back to the front of the queue
	meta->fetch(true);
	data = meta->get();
	meta->fetch(false);

	if(resp) {
		resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
		resp->set_data(data);
		resp->set_ttl(meta->ttl());
	}

	syslog(LOG_INFO, "Releasing meta\n");
	release_meta(meta);
	syslog(LOG_INFO, "Fetching content from local DONE\n");

	return RET_OK;
}

int xcache_controller::xcache_notify(struct xcache_context *c, sockaddr_x *addr,
				     socklen_t addrlen, int event)
{
	xcache_notif notif;
	notif_arrived *arrived = notif.mutable_arrived();
	std::string buffer("");

	assert(event == XCE_CHUNKARRIVED);
	notif.set_cmd(event);
	arrived->set_dag(addr, addrlen);
	notif.SerializeToString(&buffer);

	syslog(LOG_INFO, "Sent Notification to %d\n", c->notifSock);

	return send_response(c->notifSock, buffer.c_str(), buffer.length());
}

int xcache_controller::xcache_notify_contents(int context_id,
		const std::string &cid, const std::string &data)
{
	xcache_context *c = lookup_context(context_id);
	if(c == NULL) {
		syslog(LOG_ERR, "Controller: context %d not found", context_id);
		return -1;
	}
	xcache_notif notif;
	notif_contents *contents = notif.mutable_contents();
	std::string buffer("");
	notif.set_cmd(XCE_CHUNKCONTENTS);
	contents->set_cid(cid);
	contents->set_data(data.c_str(), data.length());
	notif.SerializeToString(&buffer);
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
						       args->flags);
		if (ret == RET_FAILED) {
			syslog(LOG_ERR, "Remote content fetch failed: %d", ret);
			if (args->flags & XCF_BLOCK) {
				args->resp->set_cmd(xcache_cmd::XCACHE_ERROR);
			}
		}
	} else {
		// use local machine as src dag
		// FIXME: implement this!
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

int xcache_controller::xcache_is_content_local(xcache_cmd *resp,
		xcache_cmd *cmd)
{
	// Unless we find the chunk, we return code XCACHE_ERROR to caller
	resp->set_status(xcache_cmd::XCACHE_ERR_EXISTS);
	resp->set_cmd(xcache_cmd::XCACHE_ERROR);

	// Look for the cid requested by the caller
	std::string cid(cmd->cid());
	xcache_meta *meta = acquire_meta(cid);
	if(meta && meta->state() == AVAILABLE) {
		resp->set_status(xcache_cmd::XCACHE_OK);
		resp->set_cmd(xcache_cmd::XCACHE_ISLOCAL);
	}
	if(meta != NULL) {
		release_meta(meta);
	}
	return RET_SENDRESP;
}

int xcache_controller::xcache_fetch_named_content(xcache_cmd *resp,
		xcache_cmd *cmd, int flags)
{
	// Get Publisher and Content name from cmd->content_name
	std::string name(cmd->content_name());
	size_t s_pos = name.find('/');	// position of first '/'
	std::string publisher_name = name.substr(0, s_pos);
	std::string content_name = name.substr(s_pos + 1, name.size());
	// Get reference to a Publisher object that can build NCID
	// NOTE: This object cannot sign new content. Just verify it.
	Publisher publisher(publisher_name);
	// Build DAG for retrieving the NCID
	std::string ncid_dag_str = publisher.ncid_dag(content_name);
	if(ncid_dag_str == "") {
		std::cout << "Controller::xcache_fetch_named_content "
			<< "Cannot find addr for " << name << std::endl;
		std::cout << "Controller::xcache_fetch_named_content "
			<< "RET_FAILED:" << RET_FAILED << " returning:-1" << std::endl;
		resp->set_cmd(xcache_cmd::XCACHE_ERROR);
		return RET_SENDRESP;
	}
	// Fill in the NCID DAG in the user's command
	Graph ncid_dag(ncid_dag_str);
	sockaddr_x ncid_addr;
	ncid_dag.fill_sockaddr(&ncid_addr);

	cmd->set_dag(&ncid_addr, sizeof(sockaddr_x));
	// Now call xcache_fetch_content with NCID DAG
	return xcache_fetch_content(resp, cmd, flags);
	// Verify the downloaded content against Publisher's pubkey
}

int xcache_controller::xcache_end_proxy(xcache_cmd *resp, xcache_cmd *cmd)
{
	// Set the response to be a failure
	resp->set_cmd(xcache_cmd::XCACHE_ERROR);

	assert(cmd->cmd() == xcache_cmd::XCACHE_ENDPROXY);

	//auto context_ID = cmd->context_id();
	auto proxy_id = cmd->proxy_id();

	// Lookup the proxy object from its ID
	{
		// Hold a lock so we are thread safe
		// FIXME: drop lock before waiting on thread to exit
		std::lock_guard<std::mutex> lk(_push_proxies_lock);

		auto it = push_proxies.find(proxy_id);
		auto thread_it = proxy_threads.find(proxy_id);
		if(it != push_proxies.end() && thread_it != proxy_threads.end()) {
			// Tell the proxy to stop
			push_proxies[proxy_id]->stop();
			// Join the proxy thread to exit
			proxy_threads[proxy_id]->join();
			// Now clean up data structures tracking the proxy
			push_proxies.erase(it);
			proxy_threads.erase(thread_it);
		}
	}

	// TODO: Fill in successful response here with proxy dag
	resp->set_cmd(xcache_cmd::XCACHE_ENDPROXY);
	resp->set_status(xcache_cmd::XCACHE_OK);
	return RET_SENDRESP;
}

int xcache_controller::xcache_new_proxy(xcache_cmd *resp, xcache_cmd *cmd)
{
	// Set the response to be a failure
	resp->set_cmd(xcache_cmd::XCACHE_ERROR);

	assert(cmd->cmd() == xcache_cmd::XCACHE_NEWPROXY);

	auto context_ID = cmd->context_id();

	// Start a new proxy thread, waiting for pushed chunks
	if (proxy_threads.size() >= MAX_PUSH_PROXIES) {
		std::cout << "Controller::xcache_new_proxy "
			<< "ERROR: Too many proxies already in system" << std::endl;
		return RET_SENDRESP;
	}

	// Create a new PushProxy and start it
	PushProxy *proxy = new PushProxy();
	if(proxy == NULL) {
		std::cout << "ERROR creating PushProxy object" << std::endl;
		return RET_SENDRESP;
	}
	std::cout << "Controller::xcache_new_proxy starting thread"<< std::endl;
	std::thread *proxy_thread = new std::thread(std::ref(*proxy), this, context_ID);
	if(proxy_thread == NULL) {
		std::cout << "ERROR failed to create proxy thread" << std::endl;
		return RET_SENDRESP;
	}
	int proxy_id;
	std::cout << "Proxy addr: " << proxy->addr() << std::endl;
	{
		std::lock_guard<std::mutex> lk(_push_proxies_lock);
		proxy_id = ++_proxy_id;
		std::cout << "Proxy id: " << proxy_id << std::endl;
		push_proxies[proxy_id] = proxy;
		proxy_threads[proxy_id] = proxy_thread;
	}

	// Fill in successful response here with proxy dag
	resp->set_cmd(xcache_cmd::XCACHE_NEWPROXY);
	resp->set_status(xcache_cmd::XCACHE_OK);
	resp->set_dag(proxy->addr());
	resp->set_proxy_id(proxy_id);
	return RET_SENDRESP;
}

int xcache_controller::xcache_push_chunk(xcache_cmd *resp, xcache_cmd *cmd)
{
	int remote_sock;
	int addrlen = sizeof(sockaddr_x);
	std::cout <<  "Controller: Push chunk" << std::endl;
	resp->set_cmd(xcache_cmd::XCACHE_ERROR);

	assert(cmd->cmd() == xcache_cmd::XCACHE_PUSHCHUNK);

	sockaddr_x chunk_addr;
	sockaddr_x remote_addr;

	memcpy(&chunk_addr, cmd->data().c_str(), cmd->data().length());
	memcpy(&remote_addr, cmd->dag().c_str(), cmd->dag().length());

	// A socket used for pushing the chunk to requestor
	if ((remote_sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0){
		syslog(LOG_ERR, "Xsocket not created: %s\n", strerror(errno));
		return RET_SENDRESP;
		//return -1;
	}

	Graph g(&chunk_addr);
	syslog(LOG_INFO, "Xcache: pushing: %s\n", g.dag_string().c_str());

	Graph r(&remote_addr);
	syslog(LOG_INFO, "Xcache: connecting to %s\n", r.dag_string().c_str());

	// Connect to the requestor
	if (Xconnect(remote_sock, (struct sockaddr *)&remote_addr, addrlen) < 0) {
		syslog(LOG_ERR, "Xconnect failed: %s\n", strerror(errno));
		return RET_SENDRESP;
	}

	// Now fetch the chunk from local storage and send it over
	xcache_req req;
	req.type = xcache_cmd::XCACHE_SENDCHUNK;
	req.to_sock = remote_sock;
	// FIXME: missing check to ensure chunk was available and got pushed
	send_content_remote(&req, &chunk_addr);
	Xclose(remote_sock);

	resp->set_cmd(xcache_cmd::XCACHE_PUSHCHUNK);
	resp->set_status(xcache_cmd::XCACHE_OK);
	return RET_SENDRESP;
}

std::string xcache_controller::addr2cid(sockaddr_x *addr)
{
	Graph g(addr);
	Node cid_node(g.get_final_intent());
	std::string cid = _ncid_table->to_cid(cid_node.to_string());
	return cid;
}


/*!
 * @brief Acquire metadata reference for requested content (CID/NCID)
 *
 * We convert NCIDs to corresponding CIDs and then acquire a reference
 * the CID's metadata.
 *
 * @param content_id CID/NCID string for requested content
 * @returns reference to requested metadata
 * @return NULL on error
 */
xcache_meta *
xcache_controller::acquire_meta(std::string cid)
{
	return _map->acquire_meta(cid);
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

		if (!c) {
			syslog(LOG_ALERT, "Context not found for DATASOCKET");
		} else {
			c->xcacheSock = fd;
		}
		break;

	case xcache_cmd::XCACHE_FLAG_NOTIFSOCK:
		c = lookup_context(cmd->context_id());
		if(!c) {
			syslog(LOG_ALERT, "Context not found for NOTIFSOCK");
		} else {
			c->notifSock = fd;
			ret = RET_REMOVEFD;
		}
		break;

	case xcache_cmd::XCACHE_EVICT:
	case xcache_cmd::XCACHE_STORE:
	case xcache_cmd::XCACHE_STORE_NAMED:
	case xcache_cmd::XCACHE_ISLOCAL:
	case xcache_cmd::XCACHE_FETCHCHUNK:
	case xcache_cmd::XCACHE_FETCHNAMEDCHUNK:
	case xcache_cmd::XCACHE_PUSHCHUNK:
	case xcache_cmd::XCACHE_NEWPROXY:
	case xcache_cmd::XCACHE_ENDPROXY:
	case xcache_cmd::XCACHE_READ:
		ret = RET_ENQUEUE;
		break;

	default:
		syslog(LOG_WARNING, "Unknown message received %d\n", cmd->cmd());
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

int xcache_controller::free_context_for_sock(int sockfd)
{
	int retval = -1;
	// Look up context containing sockfd in context_map
	auto it = context_map.begin();
	for(; it!=context_map.end();it++) {
		if(it->second->xcacheSock == sockfd) {
			break;
		}
	}
	// If we found a matching context for sockfd, remove it from map
	if(it != context_map.end()) {
		close(it->second->notifSock);
		context_map.erase(it);
		retval = 0;
	}
	return retval;
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

	return RET_DISCONNECT;
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
	std::string cid = meta->store_id();

	if (cid != compute_cid(data->c_str(), data->length())) {
		return false;
	}

	return true;
}

int xcache_controller::__store(xcache_meta *meta, const std::string &data)
{
	meta->lock();

	syslog(LOG_DEBUG, "[thread %lu] after locking meta map and meta\n", pthread_self());

	if (meta->valid_data(data) == false) {
		// Content Verification Failed
		syslog(LOG_ERR, "Content Verification Failed");
		meta->unlock();
		return RET_FAILED;
	}

	meta->set_length(data.size());	// size of the data in bytes
	if (__store_policy(meta) < 0) {
		syslog(LOG_ERR, "Context store failed\n");
		meta->unlock();
		return RET_FAILED;
	}
	syslog(LOG_DEBUG, "[thread %lu] after store policy\n", pthread_self());

	store_manager.store(meta, data);

	meta->set_state(AVAILABLE);

	_map->add_meta(meta);

	meta->unlock();

	// Set routes to CID and any NCID associated with the meta object
	std::vector<std::string> all_ids = meta->all_ids();
	register_meta(all_ids);

	return RET_SENDRESP;
}

/*!
 * @brief Build a DAG for provided CID/NCID cached on this host
 *
 * Build a DAG for the provided CID or NCID. It is assumed that the chunk
 * is cached on the local xcache.
 *
 *     * -------------------------> cid
 *      `--> AD -> HID -> SID--'
 *
 * @param cid is the CID or NCID that is cached locally
 *
 * @returns 0 on success
 * @returns -1 on failure
 */
int xcache_controller::cid2addr(std::string cid, sockaddr_x *sax)
{
	int rc = 0;

	struct addrinfo *ai;

	// make a direct AD-HID-SID dag for the xcache daemon
	if (Xgetaddrinfo(NULL, xcache_sid.c_str(), NULL, &ai) >= 0) {
		if (ai->ai_addr) {
			// now append the CID to it
			Node cnode(cid);
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
	std::string cid = _ncid_table->to_cid(cmd->cid());

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);

	// cid was vaidated by in the API call
	syslog(LOG_INFO, "evict called for content:%s!\n", cmd->cid().c_str());

	xcache_meta *meta = acquire_meta(cid.c_str());

	if (meta) {
		std::string c = cid;

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

/*!
 * @brief store a chunk by publisher_name/content_name
 *
 * Store given data as an NCID chunk. The Publisher must have a valid
 * key matching their name. The key is used to sign and publish the
 * named chunk and an NCID is produced for the chunk
 *
 * @returns RET_SENDRESP & a DAG for the newly stored NCID on success
 * @returns RET_FAILED on failure
 */
int xcache_controller::store_named(xcache_cmd *resp, xcache_cmd *cmd)
{
	xcache_meta *meta;
	int state = 0;
	sockaddr_x addr;
	ContentHeader *chdr;

	// Prepare a response for caller - defaulting to failure
	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);

	// Retrieve publisher name, content name and data passed by user
	std::string publisher_name = cmd->publisher_name();
	std::string content = cmd->data();
	std::string content_name = cmd->content_name();

	// FIXME: Free chdr on errors
	// Build the NCID Header from the provided request
	try {
		chdr = new NCIDHeader(content, cmd->ttl(),
				publisher_name, content_name);
		assert(chdr != NULL);
	} catch (const char *e) {
		syslog(LOG_ERR, "Error creating NCIDHeader: %s", e);
		resp->set_status(xcache_cmd::XCACHE_NCID_CALC_ERR);
		goto store_named_done;
	}
	state = 1;

	// Check if the CID for this NCID is already stored locally
	meta = acquire_meta(chdr->store_id().c_str());

	if (meta != NULL) {
		/*
		 * Meta already exists
		 */
		release_meta(meta);
		resp->set_status(xcache_cmd::XCACHE_ERR_EXISTS);
	} else {
		/*
		 * Create new Meta object and use that to store chunk
		 * NOTE: meta_map keys only by store_id == CID, never NCID
		 * but the ContentHeader value can be an NCIDHeader
		 */
		meta = new xcache_meta(chdr);
		if(meta == NULL) {
			syslog(LOG_ERR, "Error creating xcache_meta for chunk");
			goto store_named_done;
		}
		state = 2;
		meta->set_ttl(chdr->ttl());
		meta->set_created();
		meta->set_length(cmd->data().length());

		if(__store(meta, cmd->data()) == RET_FAILED) {
			syslog(LOG_ERR, "Unable to store %s", chdr->id().c_str());
			goto store_named_done;
		}
		resp->set_status(xcache_cmd::XCACHE_OK);
	}

	// Return the DAG for the CID that just got stored
	if (cid2addr(chdr->id(), &addr) == 0) {
		resp->set_dag((char *)&addr, sizeof(sockaddr_x));
		Graph g(&addr);
		syslog(LOG_INFO, "store: %s", g.dag_string().c_str());
	} else {
		resp->set_status(xcache_cmd::XCACHE_CID_NOT_FOUND);
	}

	// Keep track of NCID and corresponding CID
	_ncid_table->register_ncid(chdr->id(), chdr->store_id());

	// Successfully completed storing the NCID (internally as CID)
	syslog(LOG_INFO, "Store Finished\n");

store_named_done:
	// Remove allocated data structures on failure
	if (resp->status() != xcache_cmd::XCACHE_OK) {
		switch(state) {
			case 2: delete meta;
			case 1: delete chdr;
		};
	}
	return RET_SENDRESP;
}

int xcache_controller::store(xcache_cmd *resp, xcache_cmd *cmd, time_t ttl)
{
	xcache_meta *meta;
	int state = 0;
	int retval = RET_FAILED;
	std::string cid;
	sockaddr_x addr;

	// Build a CIDHeader so we can compute the CID
	ContentHeader *chdr = new CIDHeader(cmd->data(), ttl);
	if(chdr == NULL) {
		syslog(LOG_ERR, "Failed allocating CIDHeader for a chunk");
		goto store_done;
	}
	state = 1;

	cid = chdr->store_id();

	// Check if the CID is already stored locally
	meta = acquire_meta(cid.c_str());

	if (meta != NULL) {
		/*
		 * Meta already exists
		 */
		release_meta(meta);
		resp->set_status(xcache_cmd::XCACHE_ERR_EXISTS);
	} else {
		/*
		 * Create new Meta object and use that to store chunk
		 */
		meta = new xcache_meta(chdr);
		if(meta == NULL) {
			syslog(LOG_ERR, "Failed allocating xcache_meta for a chunk");
			goto store_done;
		}
		state = 2;
		meta->set_ttl(ttl);
		meta->set_created();
		meta->set_length(cmd->data().length());

		if(__store(meta, cmd->data()) == RET_FAILED) {
			syslog(LOG_ERR, "Failed storing chunk");
			goto store_done;
		}
	}

	// Prepare a response for caller
	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);

	// Return the DAG for the CID that just got stored
	if (cid2addr(cid, &addr) == 0) {
		resp->set_dag((char *)&addr, sizeof(sockaddr_x));
		Graph g(&addr);
		syslog(LOG_INFO, "store: %s", g.dag_string().c_str());
	} else {
		syslog(LOG_ERR, "Failed getting DAG for chunk");
		goto store_done;
	}

	retval = RET_SENDRESP;

	syslog(LOG_INFO, "Store Finished\n");

store_done:
	// On error, delete allocated data structures
	if(retval == RET_FAILED) {
		switch(state) {
			case 2: delete meta;
			case 1: delete chdr;
		};
	}
	return retval;
}

// FIXME:this should set fetching state so we can't delete the data/route
// out from under us
void xcache_controller::send_content_remote(xcache_req* req, sockaddr_x *mypath)
{
	Graph g(mypath);
	Node cid(g.get_final_intent());
	xcache_cmd resp;
	std::string header;

	// Get the content from local storage
	if (fetch_content_local(mypath, sizeof(sockaddr_x), &resp, NULL, 0)
			!= RET_OK) {
		// If chunk not found, return empty header
		header = "";
	} else {
		// cid can be CID or NCID. If CID, there may be no NCID associated
		std::string cid_str = _ncid_table->to_cid(cid.to_string());
		assert(cid_str.size() > 0);
		xcache_meta *meta = acquire_meta(cid_str);
		assert(meta);
		header = meta->content_header_str();
		release_meta(meta);
	}
	size_t remaining;
	size_t offset;
	int sent;

	// Send header length
	uint32_t header_len = htonl(header.size());
	if(Xsend(req->to_sock, (void *)&header_len, sizeof(header_len), 0) !=
			sizeof(header_len)) {
		syslog(LOG_ALERT, "Unable to send header length");
		//assert(0);
		Xclose(req->to_sock);
		return;
	}

	// Now send the header
	remaining = header.size();
	offset = 0;

	syslog(LOG_DEBUG, "Header Send Start\n");

	while (remaining > 0) {
		sent = Xsend(req->to_sock, header.c_str() + offset, remaining, 0);
		if (sent < 0) {
			syslog(LOG_ALERT, "1 Receiver Closed the connection %s", strerror(errno));
			//assert(0);
			Xclose(req->to_sock);
			return;
		}
		remaining -= sent;
		offset += sent;
	}

	// If the header size was 0, no chunk was found, so return
	if (header.size() == 0) {
		return;
	}

	// and finally, the data
	remaining = resp.data().length();
	offset = 0;

	syslog(LOG_DEBUG, "Content Send Start\n");

	while (remaining > 0) {
		sent = Xsend(req->to_sock, (char *)resp.data().c_str() + offset,
			     remaining, 0);
		syslog(LOG_DEBUG, "Sent = %d\n", sent);
		if (sent < 0) {
			syslog(LOG_ALERT, "2 Receiver Closed the connection: %s", strerror(errno));
			//assert(0);
			Xclose(req->to_sock);
			return;
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

int xcache_controller::register_meta(std::vector<std::string> &ids)
{
	int rv = -1;
	std::string empty_str("");

	for(size_t i=0;i<ids.size();i++) {

		syslog(LOG_DEBUG, "[thread %lu] Setting Route for %s.\n",
				pthread_self(), ids[i].c_str());

		rv = xr.setRoute(ids[i], DESTINED_FOR_LOCALHOST, empty_str, 0);

		syslog(LOG_DEBUG, "[thread %lu] status code %d error message %s\n",
				pthread_self(), rv, xr.cserror());
	}

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
		// Cache an in-flight chunk that is passing by
		xcache_meta *meta = acquire_meta(_ncid_table->to_cid(req->cid));
		if (meta) {
			std::string chunk((const char *)req->data, req->datalen);
			release_meta(meta);
			__store(meta, chunk);
			free(req->cid);
		}
		break;
	}

	case xcache_cmd::XCACHE_STORE_NAMED:
		// Store a named chunk provided by the user; creates NCID+CID
		ret = store_named(&resp, (xcache_cmd *)req->data);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_STORE:
		// Store a chunk provided by the user, creates CID
		ret = store(&resp, (xcache_cmd *)req->data, req->ttl);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_ISLOCAL:
		// Check if the cid is local
		cmd = (xcache_cmd *)req->data;
		ret = xcache_is_content_local(&resp, cmd);
		syslog(LOG_INFO, "xcache::is_content_local returned %d", ret);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_FETCHNAMEDCHUNK:
		// Fetch content named by the user. May be local or remote
		cmd = (xcache_cmd *)req->data;
		ret = xcache_fetch_named_content(&resp, cmd, cmd->flags());
		syslog(LOG_INFO, "xcache_fetch_named_content returned %d", ret);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_FETCHCHUNK:
		// Fetch a CID requested by the user. May be local or remote
		cmd = (xcache_cmd *)req->data;
		ret = xcache_fetch_content(&resp, cmd, cmd->flags());
		syslog(LOG_INFO, "xcache_fetch_content returned %d", ret);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_READ:
		// Read a (partial/whole) chunk from local storage
		cmd = (xcache_cmd *)req->data;
		ret = chunk_read(&resp, cmd);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_PUSHCHUNK:
		// Send requested chunk to the address given
		cmd = (xcache_cmd *)req->data;
		ret = xcache_push_chunk(&resp, cmd);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_NEWPROXY:
		// Start a proxy to accept pushed chunks
		cmd = (xcache_cmd *)req->data;
		ret = xcache_new_proxy(&resp, cmd);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_ENDPROXY:
		// Stop a proxy that was started to accept pushed chunks
		cmd = (xcache_cmd *)req->data;
		ret = xcache_end_proxy(&resp, cmd);
		if(ret == RET_SENDRESP) {
			resp.SerializeToString(&buffer);
			send_response(req->to_sock, buffer.c_str(), buffer.length());
		}
		break;
	case xcache_cmd::XCACHE_SENDCHUNK:
		// On server/router, send a chunk requested by a remote client
		send_content_remote(req, (sockaddr_x *)req->data);
		break;

	case xcache_cmd::XCACHE_EVICT:
		// Evict a chunk from local cache
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
void *xcache_controller::garbage_collector(void *)
{
	while(1) {
		sleep(GC_INTERVAL);

		meta_map *map = meta_map::get_map();
		map->walk();
	}
}

void xcache_controller::run(void)
{
	fd_set fds, allfds;
	int max, libsocket, rc, sendersocket;
	struct cache_args args;
	xcache_req *req;
	pthread_t gc_thread;

	std::vector<int> active_conns;
	std::vector<int>::iterator iter;

	// Application interface
	libsocket = xcache_create_lib_socket();

	// Arguments for the cache thread
	args.cache = &cache;
	args.ctrl = this;

	// Caching is performed by an independent thread. See cache.cc
	cache.spawn_thread(&args);

	if((rc = xr.connect()) != 0) {
		syslog(LOG_ERR, "Unable to connect to click %d \n", rc);
		return;
	}

	// Connect to Click implementation of XIA Router
	xr.setRouter(hostname);

	// A socket to receive chunk requests from remote Xcaches
	sendersocket = create_sender();

	// Start by monitoring only the libsocket and sendersocket
	FD_ZERO(&allfds);
	FD_SET(libsocket, &allfds);
	FD_SET(sendersocket, &allfds);

	// Launch all the worker threads
	for(int i = 0; i < n_threads; i++) {
		pthread_create(&threads[i], NULL, worker_thread, (void *)this);
	}

	// Start the garbage collector
	pthread_create(&gc_thread, NULL, garbage_collector, NULL);

	syslog(LOG_INFO, "Xcache::Controller Entering main loop\n");

	// Main select loop to monitor allfds
	while(true) {
		FD_ZERO(&fds);
		fds = allfds;

		max = MAX(libsocket, sendersocket);

		// FIXME: do something better
		for(iter = active_conns.begin(); iter != active_conns.end(); ++iter) {
			max = MAX(max, *iter);
		}

		// Wait for read data on all active and library sockets
		rc = Xselect(max + 1, &fds, NULL, NULL, NULL);
		if(rc == 0) {
			syslog(LOG_ERR, "Controller: Xselect returned 0 without timeout");
			continue;
		}
		if(rc < 0) {
			syslog(LOG_ERR, "Controller: Xselect failed");
			continue;
		}

		// Make sure at least one fd is set
		bool at_least_one_fd_ready = false;
		bool at_least_one_fd_processed = false;
		for(int i=0; i<max+1; i++) {
			if(FD_ISSET(i, &fds)) {
				at_least_one_fd_ready = true;
				break;
			}
		}
		if(!at_least_one_fd_ready) {
			syslog(LOG_ERR, "Controller: Xselect said fds ready but none");
			syslog(LOG_ERR, "Xselect returned %d", rc);
			continue;
		}

		// 2 new connections on libsocket when XcacheHandleInit() called
		if(FD_ISSET(libsocket, &fds)) {
			at_least_one_fd_processed = true;
			int new_connection = accept(libsocket, NULL, 0);

			// Add newly connected socket to list of fds to monitor
			if (new_connection > 0) {
				if(FD_ISSET(new_connection, &allfds)) {
					syslog(LOG_ERR, "Controller: sock already monitored %d\n",
							new_connection);
				} else {
					active_conns.push_back(new_connection);
					FD_SET(new_connection, &allfds);
				}
			}
		}

		if(FD_ISSET(sendersocket, &fds)) {
			int accept_sock;
			sockaddr_x mypath;
			socklen_t mypath_len = sizeof(mypath);
			at_least_one_fd_processed = true;

			syslog(LOG_INFO, "Accepting sender connection!");
			if((accept_sock=XacceptAs(sendersocket, (struct sockaddr *)&mypath,
										&mypath_len, NULL, NULL)) < 0) {
				syslog(LOG_ERR, "XacceptAs failed");
			} else {
				// FIXME: reply back the hop count
				req = new xcache_req();

				syslog(LOG_INFO, "XacceptAs Succeeded\n");
				Graph g(&mypath);
				g.print_graph();

				// Create a request to add to worker queue

				// Send chunk Request
				req->type = xcache_cmd::XCACHE_SENDCHUNK;
				req->from_sock = sendersocket;
				req->to_sock = accept_sock;
				req->data = malloc(mypath_len);
				memcpy(req->data, &mypath, mypath_len);
				req->datalen = mypath_len;
				// Indicates that after sending, remove the fd
				req->flags = XCFI_REMOVEFD;

				enqueue_request_safe(req);
				syslog(LOG_INFO, "Request Enqueued\n");
			}
		}

		// Look for XcacheHandle sockets ready for a read
		for(iter = active_conns.begin(); iter != active_conns.end(); ) {

			// Skip sockets not ready for read
			if(!FD_ISSET(*iter, &fds)) {
				++iter;
				continue;
			}
			at_least_one_fd_processed = true;

			char buf[XIA_MAXBUF];
			std::string buffer("");
			xcache_cmd resp, cmd;
			int ret;
			uint32_t msg_length, remaining;
			bool parse_success = false;

			// Read user command length
			ret = recv(*iter, &msg_length, sizeof(msg_length), 0);
			if(ret <= 0) {
				// Close socket and remove from active_conns and any context
				// FIXME: dangling context entry
				close(*iter);
				iter = active_conns.erase(iter);
				FD_CLR(*iter, &allfds);
				free_context_for_sock(*iter);
				continue;
			}

			remaining = ntohl(msg_length);
			assert(remaining > 0);

			while(remaining > 0) {
				ret = recv(*iter, buf, MIN(sizeof(buf), remaining), 0);
				if(ret < 0) {
					break;
				}

				buffer.append(buf, ret);
				remaining -= ret;
			}
			if(remaining != 0) {
				// Close socket and remove from active_conns and any context
				// FIXME: dangling context entry
				close(*iter);
				iter = active_conns.erase(iter);
				FD_CLR(*iter, &allfds);
				free_context_for_sock(*iter);
				continue;
			}

			parse_success = cmd.ParseFromString(buffer);

			syslog(LOG_INFO, "Controller received %lu bytes\n", buffer.length());
			if(!parse_success) {
				// Close socket and remove from active_conns and any context
				// FIXME: dangling context entry
				close(*iter);
				iter = active_conns.erase(iter);
				FD_CLR(*iter, &allfds);
				free_context_for_sock(*iter);
				continue;
			}

			// Try to see if we can process request in the fast path
			bool erasefd = false;
			ret = fast_process_req(*iter, &resp, &cmd);

			switch(ret) {
				case RET_SENDRESP:
					// Send response to XcacheHandleInit() request
					resp.SerializeToString(&buffer);
					if(send_response(*iter, buffer.c_str(), buffer.length())
							< 0) {
						syslog(LOG_ERR, "FIXME: handle failed write\n");
					} else {
						syslog(LOG_INFO, "Response sent to client\n");
					}
					break;
				case RET_REMOVEFD:
					// Notification socket. No need to monitor for read
					FD_CLR(*iter, &allfds);
					erasefd = true;
					break;
				case RET_ENQUEUE:
					// Enqueue request for slow path workers to complete
					req = new xcache_req();
					req->type = cmd.cmd();
					req->ttl = cmd.ttl();
					req->from_sock = *iter;
					req->to_sock = *iter;
					req->data = new xcache_cmd(cmd);
					req->datalen = sizeof(xcache_cmd);

					enqueue_request_safe(req);
					break;
				case RET_DISCONNECT:
					// XcacheHandleDestroy() closes *iter & removes context
					// Stop monitoring *iter for read
					FD_CLR(*iter, &allfds);
					erasefd = true;
					break;
				case RET_NORESP:
					// Usually happens when client is telling us DATASOCKET
					break;
				default:
					syslog(LOG_ERR, "Xcache: unhandled fast_process code\n");
			};

			// Advance the iterator. Erase current entry if needed.
			if(erasefd) {
				iter = active_conns.erase(iter);
			} else {
				++iter;
			}
		}
		if(at_least_one_fd_processed == false) {
			syslog(LOG_ERR, "Xcache::Controller ERROR no fd processed");
			std::cout << "Controller ERROR Xselect had an fd to process"
				<< " but we didn't find one among the ones we're looking at"
				<< std::endl;
			for(int i=0;i<max+1;i++) {
				if(FD_ISSET(i, &fds)) {
					std::cout << "FD set: " << i << std::endl;
				}
			}
		}
		assert(at_least_one_fd_processed == true);
	}
}

void xcache_controller::set_conf(struct xcache_conf *conf)
{
	hostname = std::string(conf->hostname);

	n_threads = conf->threads;
	threads = (pthread_t *)malloc(sizeof(pthread_t) * conf->threads);
}
