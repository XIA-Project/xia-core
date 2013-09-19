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
  @file session.h
  @brief Session API header file
*/

#ifndef SESSION_H
#define SESSION_H

#define DEBUG


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

enum SessionAttribute {
	kReliableDelivery 	= (1u << 0),
	kEncryption 		= (1u << 1),
	kBandwidthPriority	= (1u << 2),
	kLatencyPriority	= (1u << 3)
};

//Function list
extern int SnewContext();
extern int Sinit(int ctx, const char* sessionPath, uint32_t attributes);
extern int Sbind(int ctx, const char* name);
extern int SacceptConnReq(int ctx);
extern int Ssend(int ctx, const void* buf, size_t len);
extern int Srecv(int ctx, void* buf, size_t len);
extern int SrecvADU(int ctx, void* buf, size_t len);
extern int Sclose(int ctx);
extern bool ScheckForData(int ctx);


#ifdef __cplusplus
}
#endif

#endif /* SESSION_H */

