/* ts=4 */
/*
** Copyright 2013 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
 @file SacceptConnReq.c
 @brief Implements SacceptConnReq()
*/

#include "session.h"
#include "Sutil.h"
#include <errno.h>

using namespace std;

/**
* @brief Accept an incoming session request.
* 
* Accepts an incoming session. SacceptConnReq() will block until a connection
* request is received.
*
* @param ctx A ``listen'' context that has been bound to a name with Sbind.
*
* @return The context of the new session on success.
* @return A negative error code on failure.
*/
int SacceptConnReq(int ctx)
{
DBG("BEGIN SacceptConnReq");

	int sockfd = ctx; // for now on the client side we treat the socket fd as the context handle
	int new_sockfd;

	// make new socket for the new incoming session
	if ((new_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		ERRORF("error creating socket to session process: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = 0;

	if (bind(new_sockfd, (const struct sockaddr *)&addr, len) < 0) {
		close(new_sockfd);
		ERRORF("bind error: %s", strerror(errno));
		return -1;
	}

	// figure out which port we bound to
	if(getsockname(new_sockfd, (struct sockaddr *)&addr, &len) < 0) {
		close(new_sockfd);
		ERRORF("Error retrieving new socket's UDP port: %s", strerror(errno));
		return -1;
	}

	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::ACCEPT);
	session::SAcceptMsg *am = sm.mutable_s_accept();
	am->set_new_ctx(ntohs(addr.sin_port));

	if (proc_send(sockfd, &sm) < 0) {
		ERRORF("Error talking to session proc: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

	// process the reply from the session process
	session::SessionMsg rsm;
	if (proc_reply(sockfd, rsm) < 0) {
		ERRORF("Error getting status from session proc: %s", strerror(errno));
	} 
	if (rsm.type() != session::RETURN_CODE || rsm.s_rc().rc() != session::SUCCESS) {
		string errormsg = "Unspecified";
		if (rsm.s_rc().has_message())
			errormsg = rsm.s_rc().message();
		ERRORF("Session proc returned an error: %s", errormsg.c_str());
		return -1;
	}
	return new_sockfd;
}
