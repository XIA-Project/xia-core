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
** @file Xselect.c
** @brief implements Xselect()
*/
#include <sys/select.h>
#include "Xsocket.h"

int Xselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout)
{
	// fake stub to make apps that need select happy-ish until we get select implemented for XIA

	// assumes that user is asking for a single socket, tell them that it is ready even though we
	// don't know. Current code will block so it's sorta OK for now.
	return 1;
}