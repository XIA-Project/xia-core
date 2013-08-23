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
  @file Sutil.h
  @brief header for internal helper functions
*/  
#ifndef _SUTIL_H
#define _SUTIL_H

#include "session.pb.h"

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


int proc_send(int sockfd, session::SessionMsg *sm);
int proc_reply(int sockfd, session::SessionMsg &sm);
//int proc_reply2(int sockfd, xia::XSocketCallType *type);
int bind_to_random_port(int sockfd);

std::vector<std::string> split(const std::string &s, char delim);
std::string trim(const std::string& str);

#endif /* _SUTIL_H */
