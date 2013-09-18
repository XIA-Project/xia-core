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

#ifndef SESSION_MODULE_H
#define SESSION_MODULE_H

#include <stdio.h>
#include <pthread.h>
#include "StackInfo.h"
#include "UserLayerInfo.h"
#include "AppLayerInfo.h"
#include "TransportLayerInfo.h"
#include "NetworkLayerInfo.h"
#include "LinkLayerInfo.h"
#include "PhysicalLayerInfo.h"
#include "../session.pb.h"


/* LOGGING */

#define V_ERROR 0
#define V_WARNING 1
#define V_INFO 2
#define V_DEBUG 3

#ifdef DEBUG
#define VERBOSITY V_DEBUG
#else
#define VERBOSITY V_DEBUG
#endif

#define LOG(levelstr, color, s) fprintf(stderr, "\033[0;3%dm[ %s ]\033[0m\t[%s:%d (thread %p)]\t%s\n", color, levelstr, __FILE__, __LINE__, (void*)pthread_self(), s)
#define LOGF(levelstr, color, fmt, ...) fprintf(stderr, "\033[0;3%dm[ %s ]\033[0m\t[%s:%d (thread %p)]\t" fmt"\n", color, levelstr, __FILE__, __LINE__, (void*)pthread_self(), __VA_ARGS__) 

#if VERBOSITY >= V_INFO
#define INFO(s) LOG("INFO", 2, s)
#define INFOF(fmt, ...) LOGF("INFO", 2, fmt, __VA_ARGS__)
#else
#define INFO(s)
#define INFOF(fmt, ...)
#endif

#if VERBOSITY >= V_DEBUG
#define DBG(s) LOG("DEBUG", 4, s)
#define DBGF(fmt, ...) LOGF("DEBUG", 4, fmt, __VA_ARGS__)
#else
#define DBG(s)
#define DBGF(fmt, ...)
#endif

#if VERBOSITY >= V_ERROR
#define ERROR(s) LOG("ERROR", 1, s)
#define ERRORF(fmt, ...) LOGF("ERROR", 1, fmt, __VA_ARGS__)
#else
#define ERROR(s)
#define ERRORF(fmt, ...)
#endif

#if VERBOSITY >= V_WARNING
#define WARN(s) LOG("WARNING", 3, s)
#define WARNF(fmt, ...) LOGF("WARNING", 3, fmt, __VA_ARGS__)
#else
#define WARN(s)
#define WARNF(fmt, ...)
#endif



enum Breakpoint {
	kBindPreBind,
	kBindPostBind,

	kConnectPreConnect,
	kConnectPostConnect,
	kConnectPreSendSYN,
	kConnectPostSendSYN,
	kConnectPreAccept,
	kConnectPostAccept,
	kConnectPreReceiveSYNACK,
	kConnectPostReceiveSYNACK,

	kAcceptPreAccept,
	kAcceptPostAccept,
	kAcceptPreReceiveSYN,
	kAcceptPostReceiveSYN,
	kAcceptPreConnect,
	kAcceptPostConnect,
	kAcceptPreSendSYN,
	kAcceptPostSendSYN,

	kSendPreSend,
	kSendPostSend,
	
	kRecvPreRecv,
	kRecvPostRecv,

	kCloseReceived,
	kMigrateReceived
};

struct send_args {
	const char *buf;
	int *len;

	send_args(const char *b, int *l) : buf(b), len(l) {}
};

struct recv_args {
	char *buf;
	int *len;
	
	recv_args(char *b, int *l) : buf(b), len(l) {}
};

struct breakpoint_context {
	session::SessionInfo *sinfo;
	session::ConnectionInfo *cinfo;
	void *args;

	breakpoint_context(session::SessionInfo *s, session::ConnectionInfo *c, void *a)
		: sinfo(s), cinfo(c), args(a) {}
};


class SessionModule {
	public:
		virtual void decide(session::SessionInfo *sinfo, UserLayerInfo &userInfo, 
														 AppLayerInfo &appInfo, 
														 TransportLayerInfo &transportInfo, 
														 NetworkLayerInfo &netInfo, 
														 LinkLayerInfo &linkInfo, 
														 PhysicalLayerInfo &physInfo) = 0;

		virtual bool breakpoint(Breakpoint breakpoint, struct breakpoint_context *context, void *rv) = 0;
};


#endif /* SESSION_MODULE_H */
