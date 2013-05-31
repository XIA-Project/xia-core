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
 @file Srecv.c
 @brief Implements Srecv(), SrecvADU(), and ScheckForData()
*/

#include "session.h"
#include "Sutil.h"
#include <errno.h>

using namespace std;

int Srecv(int ctx, void* buf, size_t len, bool waitForADU)
{
LOG("BEGIN Srecv");
	int sockfd = ctx; // for now on the client side we treat the socket fd as the context handle
		
	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::RECEIVE);
	session::SRecvMsg *rm = sm.mutable_s_recv();
	rm->set_bytes_to_recv(len);
	rm->set_wait_for_adu(waitForADU);

	
	if ( proc_send(sockfd, &sm) < 0) {
		LOGF("Error talking to session proc: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

	// process the reply from the session process
	session::SessionMsg rsm;
	if (proc_reply(sockfd, rsm) < 0) {
		LOGF("Error getting status from session proc: %s", strerror(errno));
	} 
	if (rsm.type() != session::RETURN_CODE || rsm.s_rc().rc() != session::SUCCESS) {
		string errormsg = "Unspecified";
		if (rsm.s_rc().has_message())
			errormsg = rsm.s_rc().message();
		LOGF("Session proc returned an error: %s", errormsg.c_str());
		return -1;
	}
	
	// copy the returned data into the buffer
	if (!rsm.has_s_recv_ret()) {
		LOG("ERROR: Session proc returned no data");
		return -1;
	}
	int bytes  = rsm.s_recv_ret().data().size();
	const char* tempbuf = rsm.s_recv_ret().data().data();
	memcpy(buf, tempbuf, bytes); // TODO: off by one?

	return bytes;
}

int Srecv(int ctx, void* buf, size_t len) {
	return Srecv(ctx, buf, len, false);
}

int SrecvADU(int ctx, void* buf, size_t len) {
	return Srecv(ctx, buf, len, true);
}

bool ScheckForData(int ctx) {
LOG("BEGIN ScheckForData");

	int sockfd = ctx; // for now on the client side we treat the socket fd as the context handle
		
	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::CHECK_FOR_DATA);
	
	if ( proc_send(sockfd, &sm) < 0) {
		LOGF("Error talking to session proc: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

	// process the reply from the session process
	session::SessionMsg rsm;
	if (proc_reply(sockfd, rsm) < 0) {
		LOGF("Error getting status from session proc: %s", strerror(errno));
	} 
	if (rsm.type() != session::RETURN_CODE || rsm.s_rc().rc() != session::SUCCESS) {
		string errormsg = "Unspecified";
		if (rsm.s_rc().has_message())
			errormsg = rsm.s_rc().message();
		LOGF("Session proc returned an error: %s", errormsg.c_str());
		return -1;
	}
	
	// check whether or not data is available to read
	if (!rsm.has_s_check_data_ret()) {
		LOG("ERROR: Session proc didn't return data availability status");
		return -1;
	}

	return rsm.s_check_data_ret().data_available();
}
