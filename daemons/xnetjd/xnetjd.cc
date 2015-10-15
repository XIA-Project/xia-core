#include "Xsocket.h"
#include <iostream>

using namespace std;

#define XIANETJOIN_API_PORT 9882
#define MAXBUFSIZE 2048
#define MACADDRSTRLEN 18

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
		char _sender_mac_addr[6];
		char _sender_mac_addr_str[18];
	public:
		int interface() {return _interfaceID;}
		char *sender_str() {return _sender_mac_addr_str;}
		NetjoinLocalHeader(char *netjoinmsg, int msgsize) {
			_headersize = sizeof(_interfaceID) + sizeof(_sender_mac_addr);
			if(msgsize < _headersize) {
				cout << "ERROR: Netjoin message with invalid header" << endl;
				return;
			}
			_interfaceID = (int)*netjoinmsg;
			memcpy(_sender_mac_addr, netjoinmsg+1, 6);
			for(int i=0; i<6; i++) {
				snprintf(&_sender_mac_addr_str[i*3], 1, "%02x:", _sender_mac_addr[i]);
			}
			_sender_mac_addr_str[17] = '\0';
		}
};

int handle_message(char *msg, int msgsize)
{
	NetjoinLocalHeader header(msg, msgsize);
	cout << "Received pkt on interface" << header.interface();
	cout << "From " << header.sender_str() << endl;

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
	int netjsock;
	int msgsize;
	char buf[MAXBUFSIZE];
	struct sockaddr_in xianetjoinaddr;
	socklen_t xianetjoinaddrlen = sizeof(xianetjoinaddr);

	netjsock = bound_socket(config);
	if(netjsock < 0) {
		cout << "Error creating socket bound to NetJoin API port" << endl;
		return -1;
	}

	while (true) {
		bzero(buf, sizeof(buf));
		msgsize = recvfrom(netjsock, buf, config->maxbufsize(), 0,
				(struct sockaddr *)&xianetjoinaddr, &xianetjoinaddrlen);
		if(msgsize < 0) {
			cout << "Failed receiving a packet, skipping..." << endl;
		}

		if(handle_message(buf, msgsize)) {
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
	return new NetjoinConfig();
}

int main(int argc, char **argv)
{
	NetjoinConfig *config = parse_options(argc, argv);
	if(config == NULL) {
		cout << "Failed parsing options" << endl;
		return -1;
	}
	if(main_loop(config)) {
		cout << "Error: main loop ended" << endl;
		return -1;
	}
	return 0;
}
