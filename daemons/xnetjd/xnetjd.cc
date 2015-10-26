#include "Xsocket.h"
#include "xnetjd.h"
#include <map>
#include <vector>
#include <iostream>
#include <netinet/ether.h>

using namespace std;

#define MAXBUFSIZE 2048
#define MACADDRSTRLEN 18
#define LOCAL_HEADER_SIZE sizeof(uint16_t)+sizeof(struct ether_addr)

// TODO: Convert to non-global
int netjsock;
std::map<NetJoinMsgType, std::vector<struct sockaddr_in> > msg_subscribers;
std::map<NetJoinMsgType, std::vector<struct sockaddr_in> >::iterator msg_sub_iter;

class NetjoinConfig {
	private:
		int _maxbufsize;
		int _xianetjoinport;
	public:
		int maxbufsize() {return _maxbufsize;}
		int xianetjoinport() {return _xianetjoinport;}
		NetjoinConfig(int maxbufsize=MAXBUFSIZE, int xianetjoinport=XIANETJOIN_API_PORT) {
			_maxbufsize = maxbufsize;
			_xianetjoinport = xianetjoinport;
		}
};

class NetjoinLocalHeader {
	private:
		int _headersize;
		int _interfaceID;
		char _sender_mac_addr[6];      // TODO: Use struct ether_addr
		char _sender_mac_addr_str[MACADDRSTRLEN]; // TODO: Use ether_ntoa() instead.
	public:
		int interface() {return _interfaceID;}
		char *sender_str() {return _sender_mac_addr_str;}
		NetjoinLocalHeader(char *netjoinmsg, int msgsize) {
			_headersize = sizeof(_interfaceID) + sizeof(_sender_mac_addr);
			if(msgsize < _headersize) {
				cout << "ERROR: Netjoin message with invalid header" << endl;
				return;
			}
			_interfaceID = (uint16_t)*netjoinmsg;
			memcpy(_sender_mac_addr, netjoinmsg+1, 6);
			for(int i=0; i<6; i++) {
				snprintf(&_sender_mac_addr_str[i*3], 1, "%02x:", _sender_mac_addr[i]);
			}
			_sender_mac_addr_str[17] = '\0';
		}
};

int handle_subscription(char *msg, int msgsize,
		struct sockaddr_in sender, socklen_t senderlen)
{
	if(senderlen != sizeof(struct sockaddr_in)) {
		cout << "Sender's address is not a sockaddr_in" << endl;
		return -1;
	}
	// NOTE: In future, we can add subscription verification here
	// Retrieve msgtype
	NetJoinMsgType type = static_cast<NetJoinMsgType>((uint8_t) *msg);
	if(msgsize > 2) {
		cout << "Subscription request has something more than msg type" << endl;
		return -1;
	}
	// Add msgtype and sender to table
	msg_subscribers[type].push_back(sender);
	return 0;
}

int send_message(sockaddr_in dest, char *msg, int msgsize)
{
	int rc;
	int retval = 0;
	rc = sendto(netjsock, msg, msgsize, 0, (struct sockaddr*)&dest, sizeof(struct sockaddr_in));
	if(rc < msgsize) {
		cout << "Unable to send message" << endl;
		retval = -1;
	}
	return retval;
}

// Convert wire message type to WIRE_UNDERLYING_TYPE
// NOTE: INFINITE RECURSION IF TYPE NOT CHANGED AND SUCCESS(0) RETURNED
int handle_wire_message(char *msg, int msgsize)
{
	// Wire messages have a local header, extract it
	NetjoinLocalHeader header(msg, msgsize);
	cout << "Received pkt on interface" << header.interface();
	cout << "From " << header.sender_str() << endl;
	NetJoinMsgType type = static_cast<NetJoinMsgType>((uint8_t) *(msg+LOCAL_HEADER_SIZE));
	if(type == XHCP_BEACON) {
		*msg = (uint8_t) WIRE_XHCP_BEACON;
	} else {
		return -1;
	}
	return 0;
}

int handle_message(char *msg, int msgsize,
		struct sockaddr_in sender, socklen_t senderlen)
{
	NetJoinMsgType type = static_cast<NetJoinMsgType>((uint8_t) *msg);
	msg++;
	msgsize--;
	switch(type) {
		case SUBSCRIBE:
			// update msg_subscribers table
			handle_subscription(msg, msgsize, sender, senderlen);
			break;
		case WIRE_PACKET:
			// convert message type to wire version of underlying message
			if(handle_wire_message(msg, msgsize)) {
				cout << "Unable to handle wire message" << endl;
				return -1;
			}
			// re-process message
			handle_message(msg, msgsize, sender, senderlen);
			break;
		case XHCP_BEACON:
			// Deliver to beacon subscribers - xianetjoin (hardcoded for now)
			cout << "Got XHCP Beacon from an application" << endl;
			msg_sub_iter = msg_subscribers.find(XHCP_BEACON);
			if(msg_sub_iter != msg_subscribers.end()) {
				std::vector<struct sockaddr_in> subscribers = msg_subscribers[XHCP_BEACON];
				std::vector<struct sockaddr_in>::iterator it;
				for(it=subscribers.begin();it!=subscribers.end();it++) {
					// TODO: Send msg to the subscriber
					send_message(*it, msg, msgsize);
					cout << "Forwarded XHCP Beacon (most likely to Click)" << endl;
				}
			}
		case WIRE_XHCP_BEACON:
			// Forward to subscribers of WIRE_XHCP_BEACON
			cout << "Forwarding Wire XHCP Beacon" << endl;
			msg_sub_iter = msg_subscribers.find(WIRE_XHCP_BEACON);
			if(msg_sub_iter != msg_subscribers.end()) {
				std::vector<struct sockaddr_in> subscribers = msg_subscribers[WIRE_XHCP_BEACON];
				std::vector<struct sockaddr_in>::iterator it;
				for(it=subscribers.begin();it!=subscribers.end();it++) {
					// TODO: Send msg to the subscriber
					send_message(*it, msg, msgsize);
					cout << "Forwarded Wire XHCP Beacon" << endl;
				}
			}
			break;
	};

	// Read in the protobuf containing netjoin message type and message here
	return 0;
}

int bound_socket(NetjoinConfig *config)
{
	int sockfd;
	struct sockaddr_in my_addr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0) {
		cout << "Error: Unable to create socket to receive messages" << endl;
		return -1;
	}

    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(config->xianetjoinport());
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
		cout << "Error: Failed binding to netjoin API port" << endl;
        //syslog(LOG_ERR, "Failed binding to netjoin API port\n");
        return -1;
    }
	return sockfd;
}

int main_loop(NetjoinConfig *config)
{
	int msgsize;
	char buf[MAXBUFSIZE];
	struct sockaddr_in senderaddr;
	socklen_t senderaddrlen = sizeof(senderaddr);

	netjsock = bound_socket(config);
	if(netjsock < 0) {
		cout << "Error creating socket bound to NetJoin API port" << endl;
		return -1;
	}

	while (true) {
		bzero(buf, sizeof(buf));
		msgsize = recvfrom(netjsock, buf, config->maxbufsize(), 0,
				(struct sockaddr *)&senderaddr, &senderaddrlen);
		if(msgsize < 0) {
			cout << "Failed receiving a packet, skipping..." << endl;
		}

		if(handle_message(buf, msgsize, senderaddr, senderaddrlen)) {
			cout << "Failed handling message, skipping..." << endl;
		}
	}
	return 0;
}

NetjoinConfig *parse_options(int argc, char **argv)
{
	if(argc > 1) {
		cout << "Error: No arguments supported" << endl;
		return NULL;
	}
	cout << argv[1] << " Started" << endl;
	// TODO: Pass option to start forwarding XHCP_Beacons
	// for now, just add xianetjoin element as a subscriber of XHCP_Beacons
	struct sockaddr_in xianetjoinaddr;
	xianetjoinaddr.sin_family = AF_INET;
	xianetjoinaddr.sin_addr.s_addr = INADDR_ANY;
	xianetjoinaddr.sin_port = htons(XIANETJOIN_ELEMENT_PORT);
	msg_subscribers[XHCP_BEACON].push_back(xianetjoinaddr);
	return new NetjoinConfig();
}

int main(int argc, char **argv)
{
	int state = 0;
	int retval = 0;

	NetjoinConfig *config = parse_options(argc, argv);
	if(config == NULL) {
		cout << "Failed parsing options" << endl;
		retval = -1;
		goto main_done;
	}
	state = 1;
	if(main_loop(config)) {
		cout << "Error: main loop ended" << endl;
		retval = -1;
		goto main_done;
	}
main_done:
	switch(state) {
		case 1:
			delete config;
	}
	return retval;
}
