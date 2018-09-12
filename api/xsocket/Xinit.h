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

#ifndef XINIT_H
#define XINIT_H

#include "xia.pb.h"
#include <poll.h>
#include <signal.h>

//Click side: Control/data address/info
#define DEFAULT_CLICKPORT "1500"

//set xia.click sorter to sort based on these ports.


#define __PORT_LEN 6


#define CLICKPORT  DEFAULT_CLICKPORT

extern "C" {
typedef int (*socket_t)(int, int, int);
typedef int (*bind_t)(int, const struct sockaddr*, socklen_t);
typedef int (*getsockname_t)(int, struct sockaddr *, socklen_t*);
typedef int (*setsockopt_t)(int, int, int, const void*, socklen_t);
typedef int (*close_t)(int);
typedef int (*fcntl_t)(int, int, ...);
typedef int (*select_t)(int, fd_set*, fd_set*, fd_set*, struct timeval*);
typedef int (*pselect_t)(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *);

typedef ssize_t (*sendto_t)(int, const void*, size_t, int, const struct sockaddr*, socklen_t);
typedef int (*poll_t)(struct pollfd*, nfds_t, int);
typedef int (*ppoll_t)(struct pollfd*, nfds_t, const struct timespec *, const sigset_t *);
typedef ssize_t (*recvfrom_t)(int, void*, size_t, int, struct sockaddr*, socklen_t*);
typedef int (*fork_t)(void);

extern socket_t _f_socket;
extern bind_t _f_bind;
extern getsockname_t _f_getsockname;
extern setsockopt_t _f_setsockopt;
extern close_t _f_close;
extern fcntl_t _f_fcntl;
extern select_t _f_select;
extern pselect_t _f_pselect;
extern poll_t _f_poll;
extern ppoll_t _f_ppoll;
extern sendto_t _f_sendto;
extern recvfrom_t _f_recvfrom;
extern fork_t _f_fork;

extern unsigned max_api_payload;

extern __thread int _select_fd;

extern size_t api_mtu();

}

#endif
