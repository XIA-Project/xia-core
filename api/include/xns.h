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
  @file xns.h
  @brief XIA nameserver definitions
*/

#ifndef __xns_h
#define __xns_h

#ifdef __cplusplus
extern "C" {
#endif

#define NS_MAX_PACKET_SIZE 1024
#define NS_MAX_DAG_LENGTH  1024

#define NS_TYPE_REGISTER			0x01
#define NS_TYPE_QUERY				0x02
#define NS_TYPE_RQUERY				0x03
#define NS_TYPE_RESPONSE_REGISTER	0x04
#define NS_TYPE_RESPONSE_QUERY		0x05
#define NS_TYPE_RESPONSE_RQUERY		0x06
#define NS_TYPE_RESPONSE_ERROR		0x07

#define NS_FLAGS_MIGRATE 0x01

#define SID_NS "SID:1110000000000000000000000000000000001113"


typedef struct ns_pkt {
	char type;
	char flags;
	const char* name;
	const char* dag;
} ns_pkt;

extern int XregisterHost(const char *name, sockaddr_x *addr);
extern int make_ns_packet(ns_pkt *np, char *pkt, int pkt_sz);
extern void get_ns_packet(char *pkt, int sz, ns_pkt *np);

#ifdef __cplusplus
}
#endif

#endif // __xns_h
