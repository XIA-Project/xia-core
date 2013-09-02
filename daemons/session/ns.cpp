#include "session.pb.h"
#include <map>
#include <stdio.h>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

using namespace std;

#define PORT 5353

/* LOGGING */

#define V_ERROR 0
#define V_WARNING 1
#define V_INFO 2
#define V_DEBUG 3

#ifdef DEBUG
#define VERBOSITY V_DEBUG
#else
#define VERBOSITY V_DEBUG
#endif

#define LOG(levelstr, color, s) fprintf(stderr, "\033[0;3%dm[ %s ]\033[0m\t[%s:%d (thread %p)]\t%s\n", color, levelstr, __FILE__, __LINE__, (void*)pthread_self(), s)
#define LOGF(levelstr, color, fmt, ...) fprintf(stderr, "\033[0;3%dm[ %s ]\033[0m\t[%s:%d (thread %p)]\t" fmt"\n", color, levelstr, __FILE__, __LINE__, (void*)pthread_self(), __VA_ARGS__) 

#if VERBOSITY >= V_INFO
#define INFO(s) LOG("INFO", 2, s)
#define INFOF(fmt, ...) LOGF("INFO", 2, fmt, __VA_ARGS__)
#else
#define INFO(s)
#define INFOF(fmt, ...)
#endif

#if VERBOSITY >= V_DEBUG
#define DBG(s) LOG("DEBUG", 4, s)
#define DBGF(fmt, ...) LOGF("DEBUG", 4, fmt, __VA_ARGS__)
#else
#define DBG(s)
#define DBGF(fmt, ...)
#endif

#if VERBOSITY >= V_ERROR
#define ERROR(s) LOG("ERROR", 1, s)
#define ERRORF(fmt, ...) LOGF("ERROR", 1, fmt, __VA_ARGS__)
#else
#define ERROR(s)
#define ERRORF(fmt, ...)
#endif

#if VERBOSITY >= V_WARNING
#define WARN(s) LOG("WARNING", 3, s)
#define WARNF(fmt, ...) LOGF("WARNING", 3, fmt, __VA_ARGS__)
#else
#define WARN(s)
#define WARNF(fmt, ...)
#endif


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
	addr.sin_port = htons(PORT);

	if (bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sockfd);
		ERRORF("bind error: %s", strerror(errno));
		return -1;
	}

	INFOF("Listening on port %d", PORT);
	
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
				DBGF("REGISTER: %s", rm.name().c_str());
				name_to_ip[rm.name()] = rm.ip();
				name_to_port[rm.name()] = rm.port();

			} else if (nsm.has_lookup()) {  // LOOKUP
				
				session::NSLookup lm = nsm.lookup();
				DBGF("LOOKUP: %s", lm.name().c_str());

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
