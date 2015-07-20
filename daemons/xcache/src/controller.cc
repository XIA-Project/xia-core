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
#include <openssl/sha.h>

#define IGNORE_PARAM(__param) ((void)__param)

#define MAX_XID_SIZE 100

#define OK_SEND_RESPONSE 1
#define OK_NO_RESPONSE 0
#define FAILED -1

static int context_id = 0;

static int xcache_create_click_socket(int port)
{
	struct sockaddr_in si_me;
    int s;

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

    memset((char *)&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
		return -1;

    return s;
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

	LOG_INFO("[Status]\n");
	for(i = slice_map.begin(); i != slice_map.end(); ++i) {
		i->second->status();
	}
}


void xcache_controller::handle_udp(int s)
{
	IGNORE_PARAM(s);
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

	LOG_INFO("Fetching content from remote DAG = %s\n", g.dag_string().c_str());

	if(Xconnect(sock, (struct sockaddr *)&addr, daglen) < 0) {
		return FAILED;
	}

	LOG_INFO("Xcache client now connected with remote\n");

	std::string data;
	char buf[512];

	while((ret = Xrecv(sock, buf, 512, 0)) == 512) {
		data += buf;
	}
	data += buf;

	LOG_INFO("Data received = %s\n", data.c_str());
	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_data(data);

	return OK_SEND_RESPONSE;
}

int xcache_controller::fetch_content_local(xcache_cmd *resp, xcache_cmd *cmd)
{
	std::map<std::string, xcache_meta *>::iterator i = meta_map.find(cmd->cid());
	xcache_meta *meta;
	std::string data;

	LOG_INFO("Fetching content from local\n");
	
	if(i == meta_map.end())
		/* We could not find the content locally */
		return FAILED;

	meta = i->second;
	data = meta->get();

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_data(data);

	return OK_SEND_RESPONSE;
}
int xcache_controller::handle_cmd(xcache_cmd *resp, xcache_cmd *cmd)
{
	int ret = OK_NO_RESPONSE;
	xcache_cmd response;

	if(cmd->cmd() == xcache_cmd::XCACHE_STORE) {
		LOG_INFO("Received Store command\n");
		ret = store(cmd);
	} else if(cmd->cmd() == xcache_cmd::XCACHE_NEWSLICE) {
		LOG_INFO("Received Newslice command\n");
		ret = new_slice(resp, cmd);
	} else if(cmd->cmd() == xcache_cmd::XCACHE_GETCHUNK) {
		LOG_INFO("Received Getchunk command\n");
		ret = fetch_content_local(resp, cmd);
		if(ret == FAILED) {
			ret = fetch_content_remote(resp, cmd);
		}
	} else if(cmd->cmd() == xcache_cmd::XCACHE_STATUS) {
		LOG_INFO("Received Status command\n");
		status();
	}

	return ret;
}


xcache_slice *xcache_controller::lookup_slice(xcache_cmd *cmd)
{
	std::map<int32_t, xcache_slice *>::iterator i = slice_map.find(cmd->context_id());

	if(i != slice_map.end()) {
		LOG_INFO("Slice found.\n");
		return i->second;
	} else {
		/* TODO use default slice */
		LOG_INFO("Using default slice\n");
		return NULL;
	}
}

int xcache_controller::new_slice(xcache_cmd *resp, xcache_cmd *cmd)
{
	xcache_slice *slice;

	if(lookup_slice(cmd)) {
		/* Slice already exists */
		return -1;
	}

	slice = new xcache_slice(++context_id);

	// FIXME: Set policy too. slice->set_policy(cmd->cachepolicy());
	slice->set_ttl(cmd->ttl());
	LOG_INFO("Setting %Lu\n", cmd->cache_size());
	slice->set_size(cmd->cache_size());

	slice_map[slice->get_context_id()] = slice;
	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_context_id(slice->get_context_id());

	return OK_SEND_RESPONSE;
}


static std::string hex_str(unsigned char *data, int len)
{
	char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
					 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
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

int xcache_controller::store(xcache_cmd *cmd)
{
	int rv;
	xcache_slice *slice;
	xcache_meta *meta;
	std::string empty_str("");
	std::map<std::string, xcache_meta *>::iterator i = meta_map.find(cmd->cid());

	/* FIXME: Add proper errnos */
	if(i != meta_map.end()) {
		meta = i->second;
		LOG_INFO("Meta Exsits.\n");
	} else {
		/* New object - Allocate a meta */
		meta = new xcache_meta(cmd);
		meta_map[cmd->cid()] = meta;
		LOG_INFO("New Meta.\n");
	}

	slice = lookup_slice(cmd);
	if(!slice)
		return -1;

	if(slice->store(meta) < 0) {
		LOG_ERROR("Slice store failed\n");
		return -1;
	}

	std::string temp_cid("CID:");
	temp_cid += meta->get_cid();

	LOG_DEBUG("Setting Route for %s.\n", temp_cid.c_str());
	rv = xr.setRoute(temp_cid, DESTINED_FOR_LOCALHOST, empty_str, 0);
	LOG_DEBUG("Route Setting Returned %d\n", rv);

	std::cout << "Computed CID " << compute_cid(cmd->data().c_str(), cmd->data().length()) << "\n";

	return store_manager.store(meta, cmd->data());
}


void *xcache_controller::start_xcache(void *arg)
{
	xcache_controller *ctrl = (xcache_controller *)arg;
	ctrl = ctrl; //fixme
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	int xcache_sock, accept_sock;

	if ((xcache_sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		return NULL;

	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		LOG_ERROR("XmakeNewSID failed\n");
		return NULL;
	}

	if(XsetXcacheSID(xcache_sock, sid_string, strlen(sid_string)) < 0)
		return NULL;

	LOG_DEBUG("XcacheSID is %s\n", sid_string);

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
	LOG_INFO("Listening on dag: %s\n", g.dag_string().c_str());

	while(1) {
		sockaddr_x mypath;
		socklen_t mypath_len = sizeof(mypath);

		LOG_INFO("XcacheSender waiting for incoming connections\n");
		if ((accept_sock = XacceptAs(xcache_sock, (struct sockaddr *)&mypath, &mypath_len, NULL, NULL)) < 0) {
			LOG_ERROR("Xaccept failed\n");
			pthread_exit(NULL);
		}

		// FIXME: Send appropriate data, perform actual search,
		// make updates in slices / policies / stores

		LOG_INFO("Accept Success\n");
 		Graph g(&mypath);
		Node cid(g.get_final_intent());


#define DATA "IfYouReceiveThis!"
 		Xsend(accept_sock, DATA, strlen(DATA), 0);
 		Xclose(accept_sock);
	}
}

void xcache_controller::run(void)
{
	fd_set fds, allfds;
	int max, libsocket, s, rc;
	pthread_t xcache_sender;

	std::vector<int> active_conns;
	std::vector<int>::iterator iter;

	s = xcache_create_click_socket(1444);
	libsocket = xcache_create_lib_socket();

	pthread_create(&xcache_sender, NULL, start_xcache, NULL);
	if ((rc = xr.connect()) != 0) {
		LOG_ERROR("Unable to connect to click %d \n", rc);
		return;
	}

	xr.setRouter(hostname); 

	FD_ZERO(&fds);
	FD_SET(s, &allfds);
	FD_SET(libsocket, &allfds);

	LOG_INFO("Entering The Loop\n");

	while(1) {
		memcpy(&fds, &allfds, sizeof(fd_set));

		max = MAX(libsocket, s);
		for(iter = active_conns.begin(); iter != active_conns.end(); ++iter) {
			max = MAX(max, *iter);
		}

		Xselect(max + 1, &fds, NULL, NULL, NULL);

		LOG_INFO("Broken\n");
		if(FD_ISSET(s, &fds)) {
			handle_udp(s);
		}

		if(FD_ISSET(libsocket, &fds)) {
			int new_connection = accept(libsocket, NULL, 0);
			LOG_INFO("Action on libsocket\n");
			active_conns.push_back(new_connection);
			FD_SET(new_connection, &allfds);
		}

		for(iter = active_conns.begin(); iter != active_conns.end();) {
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
				if(ret == 0)
					break;

				buffer.append(buf, ret);
			} while(ret == 512);

			if(ret != 0) {
				bool parse_success = cmd.ParseFromString(buffer);
				LOG_INFO("Controller received %lu bytes\n", buffer.length());
				if(!parse_success) {
					LOG_ERROR("[ERROR] Protobuf could not parse\n");
				} else {
					if(handle_cmd(&resp, &cmd) == OK_SEND_RESPONSE) {
						resp.SerializeToString(&buffer);
						if(write(*iter, buffer.c_str(), buffer.length()) < 0) {
							LOG_ERROR("FIXME: handle return value of write\n");
						}
					}
				}
			}

			if(ret == 0) {
				close(*iter);
				FD_CLR(*iter, &allfds);
				active_conns.erase(iter);
				continue;
			}
			++iter;
		}
	}
}
