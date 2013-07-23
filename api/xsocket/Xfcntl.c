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
** @file Xfcntl.c
** @brief implements Xfcntl()
*/

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief manipulate Xsocket
**
** Performs  one  of the operations described below on the open file descriptor sockfd.
** The operation is determined by cmd.
**
** Xfcntl() can take an optional third argument.  Whether or not this argument is required is
** determined by cmd.  The required argument type is indicated in parentheses after each cmd
** name (in most cases, the required type is int, and we identify  the  argument  using  the
** name arg), or void is specified if the argument is not required.
**
** File status flags
**    F_GETFL (void)
**		Get the file access mode and the file status flags; arg is ignored.
**
**    F_SETFL (int)
**       Currently only O_NONBLOCK is allowed
**
** @param sockfd a file descriptor create by Xsocket()
** @param cmd the command to execute
** @param arg the flag to set if cmd == F_GETFL, otherwise omitted
**
** @returns socket flags if cmd == F_GETFL
** @returns 0 on success if cmd == F_SETFL
** @returns -1 on error with errno set to an error compatible with those
** returned by the standard fcntl call.
*/
int Xfcntl(int sockfd, int cmd, ...)
{

	int rc = 0;

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		errno = EBADF;
		return -1;
	}

	switch(cmd) {
		case F_GETFL:
			rc =fcntl(sockfd, cmd);
			errno = EINVAL;
			break;

		case F_SETFL:
		{
			va_list args;
			va_start(args, cmd);
			int f = va_arg(args, int);
			va_end(args);
			LOGF("fcntl flags: %08x\n", f);
			rc = fcntl(sockfd, cmd, f);
			if (rc >= 0)
				setBlocking(sockfd, f & O_NONBLOCK);
			break;
		}
		default:
			LOGF("Invalid command specified: %08x\n", cmd);
			rc = -1;
			break;
	}

	return rc;
}
