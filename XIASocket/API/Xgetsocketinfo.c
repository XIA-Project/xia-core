#include "Xsocket.h"
#include "Xinit.h"

int Xgetsocketinfo(int sockfd1, int sockfd2, struct DAGinfo *info)
{
   	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

	char buf[MAXBUFLEN];
	struct sockaddr_in their_addr;
	socklen_t addr_len;
	
    //Send a control packet 
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(CLICKCONTROLADDRESS, CLICKCONTROLPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

        // protobuf message
        xia::XSocketMsg xia_socket_msg;

        xia_socket_msg.set_type(xia::XGETSOCKETINFO);
	xia::X_Getsocketinfo_Msg *x_getsocketinfo_msg = xia_socket_msg.mutable_x_getsocketinfo();
	x_getsocketinfo_msg->set_id(sockfd2);

	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);

	if ((numbytes = sendto(sockfd1, p_buf.c_str(), p_buf.size(), 0,
					p->ai_addr, p->ai_addrlen)) == -1) {
		perror("Xgetsocketinfo(): sendto failed");
		return(-1);
	}
	freeaddrinfo(servinfo);
 

        //Process the reply
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd1, buf, MAXBUFLEN-1 , 0,
                                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("Xgetsocketinfo(): recvfrom");
                        return -1;
        }

	//protobuf message parsing
	xia_socket_msg.Clear();
	xia_socket_msg.ParseFromString(buf);

	if (xia_socket_msg.type() == xia::XGETSOCKETINFO) {
		xia::X_Getsocketinfo_Msg *x_getsocketinfo_msg = xia_socket_msg.mutable_x_getsocketinfo();
		int port = x_getsocketinfo_msg->port();
		info->port = port;
 		return 1;
	}

        return -1; 

}

