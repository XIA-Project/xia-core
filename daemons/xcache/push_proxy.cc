#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <syslog.h>
#include "Xsocket.h"
#include "Xkeys.h"
#include <iostream>
#include "controller.h"
#include "push_proxy.h"
#include "cid.h"
#include "xip.h"
#include "headers/ncid_header.h"
#include <clicknet/xia.h>
#include <clicknet/xtcp.h>

#define BUFSIZE 2048

/*!
 * @brief Create a PushProxy
 *
 * Intent is to have a PushProxy serve a client or a set of clients.
 * Initial implementation - one client, one push at a time.
 */

PushProxy::PushProxy()
{
	_proxy_addr = NULL;
	_stopping = false;
	_sid_strlen = 1+ strlen("SID:") + XIA_SHA_DIGEST_STR_LEN;
	_sid_string = (char *)malloc(_sid_strlen);
	memset(_sid_string, 0, _sid_strlen);
	if(_sid_string == NULL) {
		throw "PushProxy: Unable to locate memory for SID";
	}

	// Create a socket that this proxy will listen on
	if((_sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		throw "PushProxy: Unable to create socket";
	}

	// Assign a unique SID and build address this proxy will be on
	if(XmakeNewSID(_sid_string, _sid_strlen)) {
		throw "PushProxy: Unable to create a new SID";
	}
    struct addrinfo hints;
    struct addrinfo *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_XIA;
    if(Xgetaddrinfo(NULL, _sid_string, &hints, &ai) != 0) {
        throw "PushProxy: getaddrinfo failed";
    }
    memcpy(&_sa, (sockaddr_x *)ai->ai_addr, sizeof(sockaddr_x));
    Xfreeaddrinfo(ai);
    _proxy_addr = new Graph(&_sa);
    std::cout << "PushProxy addr: " << _proxy_addr->dag_string() << std::endl;
}

PushProxy::~PushProxy()
{
	if (_sid_string != NULL) {
		if (_sid_strlen > XIA_XID_ID_LEN) {
			XremoveSID(_sid_string);
		}
		free(_sid_string);
		_sid_string = NULL;
		_sid_strlen = 0;
	}
	Xclose(_sockfd);
}

std::string PushProxy::addr()
{
	if (_proxy_addr == NULL) {
		return "";
	}
	return _proxy_addr->dag_string();
}

// Select on the given list of sockfds
// return 0 if there is data to read
// return -1 on error or if _stopping
int PushProxy::wait_for_read(RfdList &rfdlist)
{
	std::cout << "PushProxy: waiting for read" << std::endl;
	// Convert given fd list into fd_set used by select
	fd_set rfds;
	FD_ZERO(&rfds);
	assert(rfdlist.size() > 0);
	auto maxfd = rfdlist[0];
	for (auto fd : rfdlist) {
		if(maxfd < fd) {
			maxfd = fd;
		}
		FD_SET(fd, &rfds);
	}

	// Now select waiting for data on given sockets
	struct timeval timeout;
	timeout.tv_sec = _timeoutsec;
	timeout.tv_usec = _timeoutusec;
	while(true) {
		auto ret = Xselect(maxfd+1, &rfds, nullptr, nullptr, &timeout);
		if(ret <= 0) {
			// Check if controller is asking us to stop
			if(_stopping) {
				return -1;
			}
		} else {
			std::cout << "PushProxy:: data ready to read" << std::endl;
			return 0;
		}
	}
}

void PushProxy::stop()
{
	_stopping = true;
}

void PushProxy::operator() (xcache_controller *ctrl, int context_ID)
{
	std::cout << "PushProxy: thread started" << std::endl;
	if(ctrl == NULL) {
		syslog(LOG_ERR, "PushProxy: ERROR invalid controller\n");
		std::cout << "PushProxy:: ERROR invalid controller" << std::endl;
	}
	assert(ctrl != NULL);

    if(Xbind(_sockfd, (struct sockaddr *)&_sa, sizeof(sockaddr_x)) < 0) {
        throw "Unable to bind to client address";
    }
	std::cout << "PushProxy: bound to socket" << std::endl;

    Xlisten(_sockfd, 1);

	std::cout << "PushProxy::_stopping: " << _stopping << std::endl;
    while (!_stopping) {
        int sock;
        sockaddr_x sx;
		RfdList rfd_list;
        socklen_t addrlen = sizeof(sockaddr_x);

		// Wait for a new connection
		rfd_list.push_back(_sockfd);
		if(wait_for_read(rfd_list)) break;

		// Accept new connection
        std::cout << "Socket " << _sockfd << " waiting" << std::endl;
        sock = Xaccept(_sockfd, (struct sockaddr *)&sx, &addrlen);
        if(sock < 0) {
            std::cout << "Xaccept failed. Continuing..." << std::endl;
            continue;
        }

		// We are now connected to fetching service.
		// Receive the chunk header and contents
		std::string data;
		std::unique_ptr<ContentHeader> chdr;
		std::atomic<bool> stop;
		if(xcache_get_content(sock, data, chdr, stop)) {
			std::cout << "Failed receiving content" << std::endl;
			Xclose(sock);
			continue;
		}

		// Now verify the chunk data and header
		if(!chdr->valid_data(data)) {
			std::cout << "PushProxy: Invalid chunk header/data" << std::endl;
			std::cout << "PushProxy: not saving chunk" << std::endl;
			continue;
		}
		std::cout << "PushProxy: Valid chunk received" << std::endl;

		// Send chunk contents to application that started this PushProxy
		ctrl->xcache_notify_contents(context_ID, chdr->id(), data);

		// Store the chunk
		// NOTE: Policy change. For now, we just deliver the chunk contents
		// to the application that created this PushProxy.
		/*
		xcache_meta *meta = ctrl->acquire_meta(contentID);
		if(meta != NULL) {
			ctrl->release_meta(meta);
			std::cout << "PushProxy: Chunk already in cache." << std::endl;
			std::cout << "PushProxy: don't try to cache" << std::endl;
			free(databuf);
			continue;
		}
		meta = new xcache_meta();
		if(meta == NULL) {
			std::cout << "PushProxy: Failed creating meta" << std::endl;
			free(databuf);
			continue;
		}
		meta->set_state(CACHING);
		meta->set_content_header(chdr);
		meta->set_ttl(chdr->ttl());
		meta->set_created();
		meta->set_length(chdr->content_len());
		meta->set_state(READY_TO_SAVE);
		ctrl->add_meta(meta);
		assert(ctrl->acquire_meta(contentID) != NULL);
		xcache_req *req = new xcache_req();
		req->type = xcache_cmd::XCACHE_CACHE;
		req->cid = strdup(contentID.c_str());
		req->data = databuf;
		req->datalen = chdr->content_len();
		ctrl->enqueue_request_safe(req);
		// NOTE: we don't free databuf. Controller will free it.
		std::cout << "PushProxy: chunk should be cached soon" << std::endl;
		*/
    }
}
