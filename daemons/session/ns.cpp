#include "session.pb.h"
#include <map>
#include <stdio.h>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

using namespace std;

#define DEBUG

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: INFO  %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: INFO  " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif
#define ERROR(s) fprintf(stderr, "\033[0;31m%s:%d: ERROR  %s\n\033[0m", __FILE__, __LINE__, s)
#define ERRORF(fmt, ...) fprintf(stderr, "\033[0;31m%s:%d: ERROR  " fmt"\n\033[0m", __FILE__, __LINE__, __VA_ARGS__) 



map<string, uint32_t> name_to_ip;
map<string, uint32_t> name_to_port;



int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	
	// Open socket to listen on
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		ERRORF("error creating socket to listen on: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(5353);

	if (bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sockfd);
		ERRORF("bind error: %s", strerror(errno));
		return -1;
	}
	
	// listen for REGISTER and LOOKUP messages
	while (true)
	{
		struct sockaddr_in sa;
		socklen_t len = sizeof sa;
		int rc;
		char buf[1500];
		unsigned buflen = sizeof(buf);
		memset(buf, 0, buflen);

		if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
			ERRORF("error(%d) receving message: %s", errno, strerror(errno));
		} else {
			session::NSMsg nsm;
			nsm.ParseFromString(buf);
	
			if (nsm.has_reg()) {  // REGISTER

				session::NSRegister rm = nsm.reg();
				LOGF("REGISTER: %s", rm.name().c_str());
				name_to_ip[rm.name()] = rm.ip();
				name_to_port[rm.name()] = rm.port();

			} else if (nsm.has_lookup()) {  // LOOKUP
				
				session::NSLookup lm = nsm.lookup();
				LOGF("LOOKUP: %s", lm.name().c_str());

				session::NSLookupReply *reply = nsm.mutable_reply();
				reply->set_ip(name_to_ip[lm.name()]); // TODO: handle name not found
				reply->set_port(name_to_port[lm.name()]); // TODO: handle name not found
		
				std::string p_buf;
				nsm.SerializeToString(&p_buf);

				int remaining = p_buf.size();
				const char *p = p_buf.c_str();
				rc = sendto(sockfd, p, remaining, 0, (struct sockaddr *)&sa, len);

			}
		}
	}

	return 0;
}
