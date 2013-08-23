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
DBG("BEGIN Srecv");
	int sockfd = ctx; // for now on the client side we treat the socket fd as the context handle
		
	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::RECEIVE);
	session::SRecvMsg *rm = sm.mutable_s_recv();
	rm->set_bytes_to_recv(len);
	rm->set_wait_for_adu(waitForADU);

	
	if ( proc_send(sockfd, &sm) < 0) {
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
	
	// copy the returned data into the buffer
	if (!rsm.has_s_recv_ret()) {
		ERROR("ERROR: Session proc returned no data");
		return -1;
	}
	int bytes  = rsm.s_recv_ret().data().size();
	const char* tempbuf = rsm.s_recv_ret().data().data();
	memcpy(buf, tempbuf, bytes); // TODO: off by one?

	return bytes;
}

/**
* @brief Receive data on a session.
*
* Receive a buffer of data from the previous hop in a session. If no data is
* available, Srecv() blocks.
*
* @param ctx The context handle of the session to receive on.
* @param buf A buffer where received data should be placed.
* @param len The maximum number of bytes to receive.
*
* @return The number of bytes received on success.
* @return A negative error code on failure.
*/
int Srecv(int ctx, void* buf, size_t len) {
	return Srecv(ctx, buf, len, false);
}

/**
* @brief Receive an ADU on a session.
*
* Same as Srecv(), but receives one ADU at a time. (ADU boundaries are
* determined by the sender --- each call to Ssend() generates one ADU.) If
* len is smaller than the ADU, the full ADU is not returned.
*
* @param ctx The context handle of the session to receive on.
* @param buf A buffer where received data should be placed.
* @param len The maximum number of bytes to receive.
*
* @return The number of bytes received on success.
* @return A negative error code on failure.
*/
int SrecvADU(int ctx, void* buf, size_t len) {
	return Srecv(ctx, buf, len, true);
}

/**
* @brief Check if data is available to read.
*
* Check if data is available to read on a particular session. Can be used to
* avoid a blocking call to Srecv() or SrecvADU(). (Similar to poll.)
*
* @param ctx The context handle of the session.
*
* @return true if data is available to read
* @return false otherwise
*/
bool ScheckForData(int ctx) {
DBG("BEGIN ScheckForData");

	int sockfd = ctx; // for now on the client side we treat the socket fd as the context handle
		
	// protobuf message
	session::SessionMsg sm;
	sm.set_type(session::CHECK_FOR_DATA);
	
	if ( proc_send(sockfd, &sm) < 0) {
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
	
	// check whether or not data is available to read
	if (!rsm.has_s_check_data_ret()) {
		ERROR("Session proc didn't return data availability status");
		return -1;
	}

	return rsm.s_check_data_ret().data_available();
}
