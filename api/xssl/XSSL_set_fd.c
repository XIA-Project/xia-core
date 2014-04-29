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
  @file XSSL_set_fd.c
  @brief Implements XSSL_set_fd()
*/

#include "xssl.h"


/**
* @brief Set the socket file descriptor to be used for I/O for this session.
*
* @param xssl The XSSL session object.
* @param fd  Xsocket file descriptor
*
* @return 1 on success, 0 on failure
*/
int XSSL_set_fd(XSSL *xssl, int fd) {
	// TODO: verify that fd is a valid Xsocket?
	xssl->sockfd = fd;
	return 1;
}
