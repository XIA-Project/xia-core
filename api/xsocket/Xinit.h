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

//Click side: Control/data address/info
#define DEFAULT_CLICKPORT "1500"
#define DEFAULT_CACHE_IN_PORT "1501"
#define DEFAULT_CACHE_OUT_PORT "1502"

//set xia.click sorter to sort based on these ports.


#define __PORT_LEN 6

class  __InitXSocket {
    public:
		~__InitXSocket() {};
		__InitXSocket();
        static void read_conf(const char *inifile, const char *section_name);
	    static void print_conf();
	private:
	static __InitXSocket _instance;
};

struct __XSocketConf {
  static int initialized;
  static char master_conf[BUF_SIZE];
  char click_port[__PORT_LEN];
  char cache_in_port[__PORT_LEN];
  char cache_out_port[__PORT_LEN];
};

extern struct __XSocketConf _conf;
extern struct __XSocketConf* get_conf(void);

#define CLICKPORT  (get_conf()->click_port)
#define CACHEINPORT  (get_conf()->cache_in_port)
#define CACHEOUTPORT  (get_conf()->cache_out_port)

extern "C" {
typedef int (*socket_t)(int, int, int);
typedef int (*bind_t)(int, const struct sockaddr*, socklen_t);
typedef int (*getsockname_t)(int, struct sockaddr *, socklen_t*);
typedef int (*setsockopt_t)(int, int, int, const void*, socklen_t);
typedef int (*close_t)(int);
typedef int (*fcntl_t)(int, int, ...);
typedef int (*select_t)(int, fd_set*, fd_set*, fd_set*, struct timeval*);
typedef ssize_t (*sendto_t)(int, const void*, size_t, int, const struct sockaddr*, socklen_t);
typedef int (*poll_t)(struct pollfd*, nfds_t, int);
typedef ssize_t (*recvfrom_t)(int, void*, size_t, int, struct sockaddr*, socklen_t*);
typedef int (*fork_t)(void);

extern socket_t _f_socket;
extern bind_t _f_bind;
extern getsockname_t _f_getsockname;
extern setsockopt_t _f_setsockopt;
extern close_t _f_close;
extern fcntl_t _f_fcntl;
extern select_t _f_select;
extern poll_t _f_poll;
extern sendto_t _f_sendto;
extern recvfrom_t _f_recvfrom;
extern fork_t _f_fork;

extern size_t api_mtu();

}

#endif
