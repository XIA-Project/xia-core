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
 @file Sclose.c
 @brief Implements Sclose()
*/

#include "session.h"
#include "Sutil.h"
#include <errno.h>

using namespace std;

/**
* @brief Close a session.
*
* Closes an open session. Will notify other session participants that the
* session is closing. Sclose() also closes any open transport connections
* currently being used only by this session.
*
* @param ctx The context handle for the session to close.
*
* @return A negative error code on failure.
*/
int Sclose(int ctx)
{
DBG("BEGIN Sclose");
	int rc;
	int sockfd = ctx; // for now on the client side we treat the socket fd as the context handle
		
	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::CLOSE);
	
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
	return rc;
}
