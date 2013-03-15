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
  @file xia.h
  @brief COmmon XIA structures and constants
*/

#ifndef XIA_H
#define XIA_H

#define AF_XIA		40

#define EDGES_MAX	4
#define EDGE_UNUSED 127u
#define XID_SIZE	20
#define NODES_MAX	20

typedef struct {
	unsigned int  s_type;
	unsigned char s_id[XID_SIZE];
} xid_t;

typedef struct {
	xid_t         s_xid;
	unsigned char s_edge[EDGES_MAX];
} node_t;

typedef struct {
	unsigned char s_count;
	node_t        s_addr[NODES_MAX];
} x_addr_t;

typedef struct {
	unsigned short sx_family;
	x_addr_t sx_addr;
} sockaddr_x;

#endif // XIA_H

