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
 @file Ssend.c
 @brief Implements Ssend()
*/

#include "session.h"
#include "Sutil.h"
#include <errno.h>

using namespace std;

/**
* @brief Send data on a session.
*
* Send data to the next hop in a session. Each call to Ssend() implicitly
* results in sending one ADU; if the receiver uses SrecvADU(), it will receive
* each buffer of data sent as a unit.
*
* @param ctx The handle of the session.
* @param buf The buffer of data to send.
* @param len Number of bytes to send.
*
* @return The number of bytes sent on success.
* @return A negative error code on failure.
*/
int Ssend(int ctx, const void* buf, size_t len)
{
LOG("BEGIN Ssend");
	int rc;
	int sockfd = ctx; // for now on the client side we treat the socket fd as the context handle
		
	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::SEND);
	session::SSendMsg *sendm = sm.mutable_s_send();

	// prepare data
	sendm->set_data(buf, len);
	
	if ((rc = proc_send(sockfd, &sm)) < 0) {
		LOGF("Error talking to session proc: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

	// process the reply from the session process
	session::SessionMsg rsm;
	if ((rc = proc_reply(sockfd, rsm)) < 0) {
		LOGF("Error getting status from session proc: %s", strerror(errno));
	} 
	if (rsm.type() != session::RETURN_CODE || rsm.s_rc().rc() != session::SUCCESS) {
		string errormsg = "Unspecified";
		if (rsm.s_rc().has_message())
			errormsg = rsm.s_rc().message();
		LOGF("Session proc returned an error: %s", errormsg.c_str());
		rc = -1;
	}
	// return the number of bytes sent
	if (rsm.has_s_send_ret()) { 
		rc = rsm.s_send_ret().bytes_sent();
	} else {
		LOG("WARNING: Session process did not return how many bytes were sent");
	}
	return rc;
}
