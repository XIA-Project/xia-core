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

int Sinit(int ctx, const char* forwardPath, const char* returnPath)
{
	int rc;
		
	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::INIT);
	session::SInitMsg *im = sm.mutable_s_init();

	// TODO: parse paths into protobuf msg
	
	if ((rc = click_send(sockfd, &sm)) < 0) {
		LOGF("Error talking to session proc: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

	// process the reply from the session process
	session::SessionMsg rsm;
	if ((rc = click_reply(sockfd, rsm)) < 0) {
		LOGF("Error getting status from session proc: %s", strerror(errno));
	} 
	if (rsm.type() != session::RETURN_CODE || rsm.s_rc().rc() != session::SUCCESS) {
		LOG("Session proc returned an error");
		rc = -1;
	}
	
	if (rc == 0) {
		return sockfd; // for now, treat the sockfd as the context handle
	}

	// close the control socket since the underlying Xsocket is no good
	close(sockfd);
	return -1; 
}
