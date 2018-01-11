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

	std::cout << "PushProxy being created." << std::endl;

	if((_sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		throw "PushProxy: Unable to create socket";
	}
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
    _sa = (sockaddr_x *)ai->ai_addr;
    _proxy_addr = new Graph(_sa);
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

void PushProxy::operator() (xcache_controller *ctrl)
{
	std::cout << "PushProxy:: main loop starting" << std::endl;
	if(ctrl == NULL) {
		syslog(LOG_ERR, "PushProxy: ERROR invalid controller\n");
		std::cout << "PushProxy:: ERROR invalid controller" << std::endl;
	}
	assert(ctrl != NULL);

    if(Xbind(_sockfd, (struct sockaddr *)_sa, sizeof(sockaddr_x)) < 0) {
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
		std::string headerstr(buf, chdr_len);

		// Chunk ID from sender's address
		Node src_intent_xid = g.get_final_intent();
		std::cout << "Chunk ID: " << src_intent_xid.to_string() << std::endl;

		// Build the ContentHeader object for this chunk
		ContentHeader *chdr = NULL;
		switch (src_intent_xid.type()) {
			case CLICK_XIA_XID_TYPE_CID:
				chdr = new CIDHeader(headerstr);
				break;
			case CLICK_XIA_XID_TYPE_NCID:
				chdr = new NCIDHeader(headerstr);
				break;
			default:
				assert(0);
		}
		memset(buf, 0, BUFSIZE);

		// Verify content header and compare against sender's address
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
			continue;
		}
		if (offset != data_len) {
			std::cout << "PushProxy: entire data not found" << std::endl;
			std::cout << "PushProxy: dropping connection" << std::endl;
			Xclose(sock);
			continue;
		}
        Xclose(sock);
		free(databuf);
        std::cout << "Got " << data_len << " data bytes" << std::endl;
    }
}
