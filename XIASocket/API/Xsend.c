/*
* send like datagram sending function for XIA
* IMPORTANT: Works for datagrams only
*/

#include "Xsocket.h"
#include "Xinit.h"

int Xsend(int sockfd,void *buf, size_t len, int flags)
{

	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;

	//char buffer[MAXBUFLEN];
	//struct sockaddr_in their_addr;
	//socklen_t addr_len;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;


	if ((rv = getaddrinfo(CLICKDATAADDRESS, CLICKDATAPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

        // protobuf message
        xia::XSocketMsg xia_socket_msg;

        xia_socket_msg.set_type(xia::XSEND);

        xia::X_Send_Msg *x_send_msg = xia_socket_msg.mutable_x_send();
        x_send_msg->set_payload((const char*)buf, strlen((const char*)buf));

	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);


	if ((numbytes = sendto(sockfd, p_buf.c_str(), p_buf.size(), 0, p->ai_addr, p->ai_addrlen)) == -1) {
		perror("Xsend(): sendto failed");
		return(-1);
	}
	freeaddrinfo(servinfo);
      
      /*
        //Process the reply
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buffer, MAXBUFLEN-1 , 0,
                                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("Xsend(): recvfrom");
                        return -1;
        }

	//protobuf message parsing
	xia_socket_msg.ParseFromString(buffer);

	if (xia_socket_msg.type() == xia::XSOCKET_DATA) {

 		return numbytes;
	}

        return -1; 
      */
        return numbytes;

}
