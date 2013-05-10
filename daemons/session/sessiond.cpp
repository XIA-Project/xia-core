#include "session.pb.h"
#include <map>
#include <stdio.h>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define PROCPORT 1989

#define DEBUG

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif

using namespace std;

/* DATA STRUCTURES */
map<unsigned short, session::SessionInfo*> ctx_to_session_info; 

/* FUNCTIONS */

int process_new_context_msg(session::S_New_Context_Msg ncm, struct sockaddr_in *sa, session::SessionMsg &reply) {

	// get sender's port number; use this as context handle
	unsigned short ctx = ntohs(sa->sin_port);

	// allocate an empty session state protobuf object for this session
	session::SessionInfo* info = new session::SessionInfo();
	ctx_to_session_info[ctx] = info;

	// return success
	reply.set_type(session::RETURN_CODE);
	session::S_Return_Code_Msg *rcm = reply.mutable_s_rc();
	rcm->set_rc(session::SUCCESS);

	return 0;
}


int send_reply(int sockfd, struct sockaddr_in *sa, socklen_t sa_size, session::SessionMsg *sm)
{
	int rc = 0;

	assert(sm);

	std::string p_buf;
	sm->SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();
	while (remaining > 0) {
		rc = sendto(sockfd, p, remaining, 0, (struct sockaddr *)sa, sa_size);

		if (rc == -1) {
			LOGF("socket failure: error %d (%s)", errno, strerror(errno));
			break;
		} else {
			remaining -= rc;
			p += rc;
			if (remaining > 0) {
				LOGF("%d bytes left to send", remaining);
#if 1
				// FIXME: click will crash if we need to send more than a 
				// single buffer to get the entire block of data sent. Is 
				// this fixable, or do we have to assume it will always go
				// in one send?
				LOG("click can't handle partial packets");
				rc = -1;
				break;
#endif
			}
		}	
	}

	return  (rc >= 0 ? 0 : -1);
}

int listen() {
	// Open socket to listen on
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		LOGF("error creating socket to listen on: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(PROCPORT);

	if (bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sockfd);
		LOGF("bind error: %s", strerror(errno));
		return -1;
	}


	// listen for messages (and process them)
	cout << "Listening on " << PROCPORT << endl;
	while (true)
	{
		struct sockaddr_in sa;
		socklen_t len = sizeof sa;
		int rc;
		char buf[1500];
		unsigned buflen = sizeof(buf);
		memset(buf, 0, buflen);

		if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
			LOGF("error(%d) getting reply data from session process", errno);
		} else {
			session::SessionMsg sm;
			sm.ParseFromString(buf);

			// make a blank reply message to be filled in by processing function
			session::SessionMsg reply;

			switch(sm.type())
			{
				case session::NEW_CONTEXT:
					rc = process_new_context_msg(sm.s_new_context(), &sa, reply);
					break;
				default:
					LOG("Unrecognized protobuf message");
			}

			// send the reply that was filled in by the processing function
			if (rc < 0) {
				LOG("Error processing message");
			} else {
				if ((rc = send_reply(sockfd, &sa, len, &reply)) < 0) {
					LOG("Error sending reply");
				}
			}
		}
		cout << "Num sessions: " << ctx_to_session_info.size() << endl;
	}
}




int main(int argc, char *argv[]) {
	listen();
	return 0;
}
