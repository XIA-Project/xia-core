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
#include "ncid_header.h"
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
	/*
	if (_sid_string != NULL) {
		if (_sid_strlen > XIA_XID_ID_LEN) {
			XremoveSID(_sid_string);
		}
		free(_sid_string);
		_sid_string = NULL;
		_sid_strlen = 0;
	}
	Xclose(_sockfd);
	*/
}

std::string PushProxy::addr()
{
	if (_proxy_addr == NULL) {
		return "";
	}
	return _proxy_addr->dag_string();
}

void PushProxy::operator() (xcache_controller *ctrl, int context_ID)
{
	if(ctrl == NULL) {
		syslog(LOG_ERR, "PushProxy: ERROR invalid controller\n");
		std::cout << "PushProxy:: ERROR invalid controller" << std::endl;
	}
	assert(ctrl != NULL);

    if(Xbind(_sockfd, (struct sockaddr *)&_sa, sizeof(sockaddr_x)) < 0) {
        throw "Unable to bind to client address";
    }

    Xlisten(_sockfd, 1);

    while (1) {
        int sock;
        sockaddr_x sx;
        socklen_t addrlen = sizeof(sockaddr_x);
        char buf[BUFSIZE];
		memset(buf, 0, BUFSIZE);

		// Accept new connection
        std::cout << "Socket " << _sockfd << " waiting" << std::endl;
        sock = Xaccept(_sockfd, (struct sockaddr *)&sx, &addrlen);
        if(sock < 0) {
            std::cout << "Xaccept failed. Continuing..." << std::endl;
            continue;
        }

		std::cout << "Connected to a publisher" << std::endl;
		std::cout << "Addrlen: " << addrlen << std::endl;
		// A producer at addr 'sx' is pushing a chunk to us
        Graph g(&sx);
        std::cout << "Socket " << sock << " connected to " <<
            g.dag_string() << std::endl;

		// Get the content header length
		uint32_t chdr_len = 0;
		int count = 0;
		count = Xrecv(sock, &chdr_len, sizeof(chdr_len), 0);
		if (count != sizeof(chdr_len)) {
			std::cout << "PushProxy: Failed getting header len." << std::endl;
			std::cout << "Dropping connection." << std::endl;
			Xclose(sock);
			continue;
		}
		chdr_len = ntohl(chdr_len);
		std::cout << "Pushproxy: header length: " << chdr_len << std::endl;

		// Get the content header
		int remaining = chdr_len;
		size_t offset = 0;
		while(remaining) {
			count = Xrecv(sock, buf+offset, remaining, 0);
			offset += count;
			remaining -= count;
			if (count == 0) {
				break;
			}
		}
		if (remaining != 0) {
			std::cout << "PushProxy: error getting header." << std::endl;
			std::cout << "PushProxy: got " << chdr_len - remaining << std::endl;
			std::cout << "PushProxy: expected " << chdr_len << std::endl;
			std::cout << "PushProxy: dropping connection" << std::endl;
			Xclose(sock);
			continue;
		}
		if (offset != chdr_len) {
			std::cout << "PushProxy: entire header not found" << std::endl;
			std::cout << "PushProxy: dropping connection" << std::endl;
			Xclose(sock);
			continue;
		}

		// Parse the header protobuf to determine type of header
		std::string headerstr(buf, chdr_len);
		ContentHeaderBuf chdr_buf;
		if(chdr_buf.ParseFromString(headerstr) == false) {
			assert(0);
		}

		// Now create the header object for the received header
		ContentHeader *chdr = NULL;
		if(chdr_buf.has_cid_header()) {
			chdr = new CIDHeader(headerstr);
		} else if(chdr_buf.has_ncid_header()) {
			chdr = new NCIDHeader(headerstr);
		} else {
			assert(0);
		}

		memset(buf, 0, BUFSIZE);
		std::string contentID(chdr->id());

		// Verify content header against sender's address
		// NOT NEEDED because sender can be anyone, as long as they
		// provide a valid chunk
		/*
		if(src_intent_xid.to_string().compare(contentID) != 0) {
			std::cout << "PushProxy: headerSID != src DAG SID" << std::endl;
			std::cout << "PushProxy: " << src_intent_xid.to_string()
				<< " != " << contentID << std::endl;
			std::cout << "PushProxy: dropping connection" << std::endl;
			Xclose(sock);
			continue;
		}
		*/

		// FIXME: add code to verify if this chunk should be accepted

		// Now download content
		size_t data_len = chdr->content_len();
		std::cout << "PushProxy: getting data bytes: " << data_len << std::endl;
		remaining = data_len;
		offset = 0;
		char *databuf = (char *)calloc(1, data_len);
		if (databuf == NULL) {
			std::cout << "PushProxy: failed allocating " << data_len
				<< " bytes for pushed chunk data" << std::endl;
			std::cout << "PushProxy: dropping connection" << std::endl;
			Xclose(sock);
			continue;
		}
		while(remaining) {
			// TODO: Add an Xselect here in case data never comes
			count = Xrecv(sock, databuf+offset, remaining, 0);
			std::cout << "Got bytes: " << count << std::endl;
			offset += count;
			remaining -= count;
			if (count == 0) {
				break;
			}
		}
		if (remaining != 0) {
			std::cout << "PushProxy: error getting content" << std::endl;
			std::cout << "PushProxy:: dropping connection" << std::endl;
			Xclose(sock);
			free(databuf);
			continue;
		}
		if (offset != data_len) {
			std::cout << "PushProxy: entire data not found" << std::endl;
			std::cout << "PushProxy: dropping connection" << std::endl;
			Xclose(sock);
			free(databuf);
			continue;
		}
        Xclose(sock);
        std::cout << "Got " << data_len << " data bytes" << std::endl;
		// Now verify the chunk data and header
		std::string data(databuf, data_len);
		if(!chdr->valid_data(data)) {
			std::cout << "PushProxy: Invalid chunk header/data" << std::endl;
			std::cout << "PushProxy: not saving chunk" << std::endl;
			free(databuf);
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
