/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
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
 @file Sinit.c
 @brief Implements Sinit()
*/

#include "session.h"
#include "Sutil.h"
#include <errno.h>

using namespace std;

/**
* @brief Initializes a session.
*
* Initiates a session (akin to "connect" in the standard sockets API). Begins by
* opening a transport connection with the next application in the session path
* and passing on the required session information for the next hop to continue
* session setup by connecting to the following hop.
*
* @param ctx The context (like a session-layer socket).
* @param sessionPath A comma separated list (as a stirng) of application names
* specifying through which applications session data should pass.
*
* @return A negative value on failure.
*/
int Sinit(int ctx, const char* sessionPath)
{
DBG("BEGIN Sinit");
	int rc;
	int sockfd = ctx; // for now on the client side we treat the socket fd as the context handle
		
	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::INIT);
	session::SInitMsg *im = sm.mutable_s_init();

	// parse paths into protobuf msg
	im->set_session_path(sessionPath);
	
	if ((rc = proc_send(sockfd, &sm)) < 0) {
		ERRORF("Error talking to session proc: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

	// process the reply from the session process
	session::SessionMsg rsm;
	if ((rc = proc_reply(sockfd, rsm)) < 0) {
		ERRORF("Error getting status from session proc: %s", strerror(errno));
	} 
	if (rsm.type() != session::RETURN_CODE || rsm.s_rc().rc() != session::SUCCESS) {
		string errormsg = "Unspecified";
		if (rsm.s_rc().has_message())
			errormsg = rsm.s_rc().message();
		ERRORF("Session proc returned an error: %s", errormsg.c_str());
		rc = -1;
	}
	
	if (rc >= 0) {
		return sockfd; // for now, treat the sockfd as the context handle
	}

	// close the control socket since the underlying Xsocket is no good
	close(sockfd);
	return -1; 
}
