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
#include <errno.h>
#include "controller.h"
#include <iostream>
#include "logger.h"
#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include "xcache_sock.h"
#include "cid.h"
#include <openssl/sha.h>

#define IGNORE_PARAM(__param) ((void)__param)

#define MAX_XID_SIZE 100

#define OK_SEND_RESPONSE 1
#define OK_NO_RESPONSE 0
#define FAILED -1

DEFINE_LOG_MACROS(CTRL)

static std::string hex_str(unsigned char *data, int len)
{
	char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
					 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	std::string s(len * 2, ' ');
	for (int i = 0; i < len; ++i) {
		s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
		s[2 * i + 1] = hexmap[data[i] & 0x0F];
	}
	return s;
}

static std::string compute_cid(const char *data, size_t len)
{
	unsigned char digest[SHA_DIGEST_LENGTH];

	SHA1((unsigned char *)data, len, digest);

	return hex_str(digest, SHA_DIGEST_LENGTH);
}

static int xcache_create_lib_socket(void)
{
	struct sockaddr_un my_addr;
	int s;
	char socket_name[512];

	s = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	my_addr.sun_family = AF_UNIX;
	if(get_xcache_sock_name(socket_name, 512) < 0) {
		return -1;
	}

	remove(socket_name);

	strcpy(my_addr.sun_path, socket_name);

	bind(s, (struct sockaddr *)&my_addr, sizeof(my_addr));
	listen(s, 100);

	return s;
}

void xcache_controller::status(void)
{
	std::map<int32_t, xcache_slice *>::iterator i;

	LOG_CTRL_INFO("[Status]\n");
	for(i = slice_map.begin(); i != slice_map.end(); ++i) {
		i->second->status();
	}
}

int xcache_controller::fetch_content_remote(xcache_cmd *resp, xcache_cmd *cmd)
{
	int ret, sock;
	socklen_t daglen;
	sockaddr_x addr;

	daglen = cmd->dag().length();
	daglen = daglen;

	memcpy(&addr, cmd->dag().c_str(), cmd->dag().length());
	Graph g(&addr);

	sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if(sock < 0) {
		return FAILED;
	}

	LOG_CTRL_INFO("Fetching content from remote DAG = %s\n", g.dag_string().c_str());

	if(Xconnect(sock, (struct sockaddr *)&addr, daglen) < 0) {
		return FAILED;
	}

	LOG_CTRL_INFO("Xcache client now connected with remote\n");

	std::string data;
	char buf[XIA_MAXBUF] = {0};

	Node expected_cid(g.get_final_intent());
	xcache_meta *meta = new xcache_meta(expected_cid.id_string());
	
	meta->set_FETCHING();

	// FIXME: This is really bad way of receiving data.
	do {
		ret = Xrecv(sock, buf, XIA_MAXBUF, 0);
		if(ret <= 0)
			break;

		std::string temp(buf + sizeof(struct cid_header), ret - sizeof(struct cid_header));

		data += temp;
	} while(1);

	std::string computed_cid = compute_cid(data.c_str(), data.length());
	xcache_slice *slice = lookup_slice(cmd->context_id());

	if(!slice) {
		LOG_CTRL_ERROR("Slice Lookup Failed\n");
		delete meta;
		return FAILED;
	}

	LOG_CTRL_INFO("DATA = %s", data.c_str());

	__store(slice, meta, (const std::string *)&data);

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_data(data);

	return OK_SEND_RESPONSE;
}

int xcache_controller::fetch_content_local(xcache_cmd *resp, std::string cid)
{
	xcache_meta *meta;
	std::string data;

	meta = acquire_meta(cid);
	LOG_CTRL_INFO("Fetching content from local\n");
	
	if(!meta) {
		/* We could not find the content locally */
		return FAILED;
	}


	LOG_CTRL_INFO("Fetching content from local 1\n");
	if(!meta->is_AVAILABLE()) {
		release_meta(meta);
		return FAILED;
	}

	LOG_CTRL_INFO("Fetching content from local 2\n");
	data = meta->get();

	
	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_data(data);
	LOG_CTRL_INFO("Fetching content from local 3\n");

	release_meta(meta);

	LOG_CTRL_INFO("Fetching content from local 4\n");
	return OK_SEND_RESPONSE;
}

int xcache_controller::handle_cmd(xcache_cmd *resp, xcache_cmd *cmd)
{
	int ret = OK_NO_RESPONSE;
	xcache_cmd response;

	if(cmd->cmd() == xcache_cmd::XCACHE_STORE) {
		LOG_CTRL_INFO("Received Store command\n");
		ret = store(resp, cmd);
	} else if(cmd->cmd() == xcache_cmd::XCACHE_NEWSLICE) {
		LOG_CTRL_INFO("Received Newslice command\n");
		ret = new_slice(resp, cmd);
	} else if(cmd->cmd() == xcache_cmd::XCACHE_GETCHUNK) {
		LOG_CTRL_INFO("Received Getchunk command\n");
		ret = fetch_content_local(resp, cmd->cid());
		if(ret == FAILED) {
			ret = fetch_content_remote(resp, cmd);
		}
	} else if(cmd->cmd() == xcache_cmd::XCACHE_GET_STATUS) {
		LOG_CTRL_INFO("Received Status command\n");
		status();
	}

	return ret;
}


xcache_slice *xcache_controller::lookup_slice(int context_id)
{
	std::map<int32_t, xcache_slice *>::iterator i = slice_map.find(context_id);

	if(i != slice_map.end()) {
		LOG_CTRL_INFO("Slice found.\n");
		return i->second;
	} else {
		i = slice_map.find(0);
		if(i == slice_map.end()) {
			LOG_CTRL_ERROR("Should Not Happen. Default Slice Lookup Failed.\n");
			return NULL;
		}
		return i->second;
	}
}

int xcache_controller::new_slice(xcache_cmd *resp, xcache_cmd *cmd)
{
	xcache_slice *slice;

	slice = new xcache_slice(++context_id);

	// FIXME: Set policy too. slice->set_policy(cmd->cachepolicy());
	slice->set_ttl(cmd->ttl());
	LOG_CTRL_INFO("Setting %Lu\n", cmd->cache_size());
	slice->set_size(cmd->cache_size());

	slice_map[slice->get_context_id()] = slice;
	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_context_id(slice->get_context_id());

	return OK_SEND_RESPONSE;
}

int xcache_controller::__store(xcache_slice *slice, xcache_meta *meta, const std::string *data)
{
	lock_meta_map();
	meta->lock();

	meta_map[meta->get_cid()] = meta;

	if(!slice) {
		slice = lookup_slice(0);
	}

	if(slice->store(meta) < 0) {
		LOG_CTRL_ERROR("Slice store failed\n");
		meta->unlock();
		unlock_meta_map();
		return FAILED;
	}

	register_meta(meta);
	store_manager.store(meta, data);

	LOG_CTRL_INFO("STORING %s\n", data->c_str());

	meta->set_AVAILABLE();
	meta->unlock();
	unlock_meta_map();

	return OK_SEND_RESPONSE;
}

int xcache_controller::store(xcache_cmd *resp, xcache_cmd *cmd)
{
	xcache_slice *slice;
	xcache_meta *meta;
	std::string cid = compute_cid(cmd->data().c_str(), cmd->data().length());

	meta = acquire_meta(cid);

	/* FIXME: Add proper errnos */
	if(meta) {
		LOG_CTRL_ERROR("Meta Exists.\n");
		resp->set_cmd(xcache_cmd::XCACHE_ERROR);
		resp->set_status(xcache_cmd::XCACHE_ERR_EXISTS);
		release_meta(meta);
		return OK_SEND_RESPONSE;
	}

	/* New object - Allocate a meta */
	meta = new xcache_meta(cid);


	slice = lookup_slice(cmd->context_id());
	if(!slice) {
		return FAILED;
	}

	if(__store(slice, meta, &cmd->data()) == FAILED) {
		return FAILED;
	}

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_cid(cid);

	LOG_CTRL_INFO("Store Finished\n");
	return OK_SEND_RESPONSE;
}

void *xcache_controller::start_xcache(void *arg)
{
	xcache_controller *ctrl = (xcache_controller *)arg;
	ctrl = ctrl; //fixme
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	int xcache_sock, accept_sock;

	if((xcache_sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		return NULL;

	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		LOG_CTRL_ERROR("XmakeNewSID failed\n");
		return NULL;
	}

	if(XsetXcacheSID(xcache_sock, sid_string, strlen(sid_string)) < 0)
		return NULL;

	LOG_CTRL_DEBUG("XcacheSID is %s\n", sid_string);

	struct addrinfo *ai;

	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0)
		return NULL;

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;

	if (Xbind(xcache_sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(xcache_sock);
		return NULL;
	}

	Xlisten(xcache_sock, 5);

	Graph g(dag);
	LOG_CTRL_INFO("Listening on dag: %s\n", g.dag_string().c_str());

	while(1) {
		sockaddr_x mypath;
		socklen_t mypath_len = sizeof(mypath);

		LOG_CTRL_INFO("XcacheSender waiting for incoming connections\n");
		if ((accept_sock = XacceptAs(xcache_sock, (struct sockaddr *)&mypath, &mypath_len, NULL, NULL)) < 0) {
			LOG_CTRL_ERROR("Xaccept failed\n");
			pthread_exit(NULL);
		}

		// FIXME: Send appropriate data, perform actual search,
		// make updates in slices / policies / stores

		LOG_CTRL_INFO("Accept Success\n");
 		Graph g(&mypath);
		Node cid(g.get_final_intent());
		xcache_cmd resp;

		LOG_CTRL_INFO("CID = %s\n", cid.id_string().c_str());

		if(ctrl->fetch_content_local(&resp, cid.id_string()) < 0) {
			LOG_CTRL_ERROR("This should not happen.\n");
		}

//		LOG_CTRL_INFO("Sending %s[%d]\n", resp.data().c_str(), resp.data().length());

		size_t remaining = resp.data().length();
		size_t offset = 0;

		while(remaining > 0) {
			char buf[XIA_MAXBUF];
			size_t to_send = MIN(XIA_MAXBUF, remaining);
			struct cid_header header;
			size_t header_size = sizeof(header);

			if(to_send + header_size > XIA_MAXBUF)
				to_send -= header_size;

			header.offset = offset;
			header.length = to_send;
			header.total_length = resp.data().length();
			strcpy(header.cid, cid.id_string().c_str());

			memcpy(buf, &header, header_size);
			memcpy(buf + header_size, resp.data().c_str() + offset, to_send);

			remaining -= to_send;

			Xsend(accept_sock, buf, to_send + header_size, 0);

			offset += to_send;
		}

 		Xclose(accept_sock);
	}
}

#define API_CHUNKSIZE 129
static int send_response(int fd, const char *buf, size_t len)
{
	int ret = 0;
	size_t off = 0;

#ifndef MIN
#define MIN(__x, __y) ((__x) < (__y) ? (__x) : (__y))
#endif

	while(off < len) {
		size_t remaining = len - off;

		if((ret = write(fd, buf + off, MIN(remaining, API_CHUNKSIZE))) < 0) {
			return ret;
		}
		off += API_CHUNKSIZE;
	}

	return ret;
}

int xcache_controller::register_meta(xcache_meta *meta)
{
	int rv;
	std::string empty_str("");
	std::string temp_cid("CID:");

	temp_cid += meta->get_cid();
	
	LOG_CTRL_DEBUG("Setting Route for %s.\n", temp_cid.c_str());
	rv = xr.setRoute(temp_cid, DESTINED_FOR_LOCALHOST, empty_str, 0);
	LOG_CTRL_DEBUG("Route Setting Returned %d\n", rv);

	return rv;
}

void xcache_controller::run(void)
{
	fd_set fds, allfds;
	int max, libsocket, rc;
	pthread_t xcache_sender;
	struct cache_args args;

	std::vector<int> active_conns;
	std::vector<int>::iterator iter;

	libsocket = xcache_create_lib_socket();

	args.cache = &cache;
	args.cache_in_port = getXcacheInPort();
	args.cache_out_port = getXcacheOutPort();
	args.ctrl = this;
	
	cache.spawn_thread(&args);

	pthread_create(&xcache_sender, NULL, start_xcache, (void *)this);
	if ((rc = xr.connect()) != 0) {
		LOG_CTRL_ERROR("Unable to connect to click %d \n", rc);
		return;
	}

	xr.setRouter(hostname); 

	FD_ZERO(&fds);
	FD_SET(libsocket, &allfds);

	LOG_CTRL_INFO("Entering The Loop\n");

repeat:
	memcpy(&fds, &allfds, sizeof(fd_set));

	max = libsocket;
	for(iter = active_conns.begin(); iter != active_conns.end(); ++iter) {
		max = MAX(max, *iter);
	}

	Xselect(max + 1, &fds, NULL, NULL, NULL);

	LOG_CTRL_INFO("Broken\n");

	if(FD_ISSET(libsocket, &fds)) {
		int new_connection = accept(libsocket, NULL, 0);
		LOG_CTRL_INFO("Action on libsocket\n");
		active_conns.push_back(new_connection);
		FD_SET(new_connection, &allfds);
	}

	for(iter = active_conns.begin(); iter != active_conns.end(); ) {
		if(!FD_ISSET(*iter, &fds)) {
			++iter;
			continue;
		}

		char buf[512] = "";
		std::string buffer("");
		xcache_cmd resp, cmd;
		int ret;

		do {
			ret = recv(*iter, buf, 512, 0);
			LOG_CTRL_INFO("Recv returned %d\n", ret);
			if(ret <= 0)
				break;

			buffer.append(buf, ret);
		} while(ret == 512);

		if(ret <= 0) {
			LOG_CTRL_DEBUG("Client Disconnected.\n");
			close(*iter);
			FD_CLR(*iter, &allfds);
			active_conns.erase(iter);
			continue;
		} else if(ret != 0) {
			bool parse_success = cmd.ParseFromString(buffer);
			LOG_CTRL_INFO("Controller received %lu bytes\n", buffer.length());
			if(!parse_success) {
				LOG_CTRL_ERROR("[ERROR] Protobuf could not parse\n");
			} else if(handle_cmd(&resp, &cmd) == OK_SEND_RESPONSE) {
				resp.SerializeToString(&buffer);
				if(send_response(*iter, buffer.c_str(), buffer.length()) < 0) {
					LOG_CTRL_ERROR("FIXME: handle return value of write\n");
				} else {
					LOG_CTRL_INFO("Data sent to client\n");
				}
			}
		}
		++iter;
	}
	goto repeat;
}
