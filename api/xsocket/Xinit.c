
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
 @internal
 @file Xinit.c
 @brief Implements internal support functions
*/

#include <sys/types.h>
#include <unistd.h>

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <stdlib.h>
#include "minIni.h"
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <signal.h>
#include "state.h"

#ifdef __mips__
	#define LIBNAME "libc.so.0"
#else
	#define LIBNAME	"libc.so.6"
#endif

using namespace std;

extern "C" {

socket_t _f_socket;
bind_t _f_bind;
getsockname_t _f_getsockname;
setsockopt_t _f_setsockopt;
close_t _f_close;
fcntl_t _f_fcntl;
select_t _f_select;
pselect_t _f_pselect;
poll_t _f_poll;
ppoll_t _f_ppoll;
sendto_t _f_sendto;
recvfrom_t _f_recvfrom;
fork_t _f_fork;

// each thread will use a single socket to talk to click from select/poll
__thread int _select_fd = -1;


unsigned max_api_payload = XIA_MAXBUF;

size_t mtu_internal = 0;
size_t mtu_wire = 1500;

size_t api_mtu()
{
	if (mtu_internal == 0) {
		struct ifreq ifr;

		int sock = (_f_socket)(AF_INET, SOCK_DGRAM, 0);

		strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);
		ifr.ifr_addr.sa_family = AF_INET;
		ioctl(sock, SIOCGIFMTU, &ifr);
		(_f_close)(sock);

		mtu_internal = ifr.ifr_mtu;
		// LOGF("API MTU = %d\n", ifr.ifr_mtu);
	}

	return mtu_internal;
}


static void set_api_payload_size()
{
	char *maxbuf = secure_getenv("XIA_MAXBUF");
	max_api_payload = XIA_MAXBUF;

	if (maxbuf) {
		char *err;
		unsigned m = strtoul(maxbuf, &err, 10);

		printf("e=%s m=%u err=%s\n", maxbuf, m, err);

		if (m > 0) {
			if (m < 4096) {
				max_api_payload = 4096;
			} else if (m < XIA_MAXBUF) {
				max_api_payload = m;
			}
		}
	}
}


void cleanup()
{
	// loop through left open sockets and close them
	SocketMap *socketmap = SocketMap::getMap();
	SMap *sockets = socketmap->smap();
	SMap::iterator it;

	for (it = sockets->begin(); it != sockets->end(); it++) {
		Xclose(it->first);
	}
}

#if 0
struct sigaction sa_new;
struct sigaction sa_term;
struct sigaction sa_int;

void handler(int sig)
{
	cleanup();

	// chain to the old handler if it exists
	switch (sig) {
		case SIGTERM:
			if (sa_term.sa_handler)
				(sa_term.sa_handler)(sig);
			break;
		case SIGINT:
			if (sa_int.sa_handler)
				(sa_int.sa_handler)(sig);
			break;
		default:
			break;
	}
}
#endif


// Run at library load time to initialize function pointers
void __attribute__ ((constructor)) api_init()
{
	void *handle = dlopen(LIBNAME, RTLD_LAZY);

	// FIXME: add code to find the name of the library instead of hard coding it.
	if (!handle) {
		fprintf(stderr, "Unable to locate %s. Is there a different version of libc?", LIBNAME);
		exit(-1);
	}

	if(!(_f_socket = (socket_t)dlsym(handle, "socket")))
		printf("can't find socket!\n");
	if(!(_f_bind = (bind_t)dlsym(handle, "bind")))
		printf("can't find bind!\n");
	if(!(_f_getsockname = (getsockname_t)dlsym(handle, "getsockname")))
		printf("can't find getsockname!\n");
	if(!(_f_setsockopt = (setsockopt_t)dlsym(handle, "setsockopt")))
		printf("can't find setsockopt!\n");
	if(!(_f_close = (close_t)dlsym(handle, "close")))
		printf("can't find close!\n");
	if(!(_f_fcntl = (fcntl_t)dlsym(handle, "fcntl")))
		printf("can't find fcntl!\n");
	if(!(_f_select = (select_t)dlsym(handle, "select")))
		printf("can't find select!\n");
	if(!(_f_pselect = (pselect_t)dlsym(handle, "pselect")))
		printf("can't find pselect!\n");
	if(!(_f_poll = (poll_t)dlsym(handle, "poll")))
		printf("can't find poll!\n");
	if(!(_f_ppoll = (ppoll_t)dlsym(handle, "ppoll")))
		printf("can't find ppoll!\n");
	if(!(_f_sendto = (sendto_t)dlsym(handle, "sendto")))
		printf("can't find sendto!\n");
	if(!(_f_recvfrom = (recvfrom_t)dlsym(handle, "recvfrom")))
		printf("can't find recvfrom!\n");
	if(!(_f_fork = (fork_t)dlsym(handle, "fork")))
		printf("can't find fork!\n");

	api_mtu();
	set_api_payload_size();


#if 0
	memset (&sa_new, 0, sizeof (struct sigaction));
	sigemptyset (&sa_new.sa_mask);
	sa_new.sa_handler = handler;
	sa_new.sa_flags = 0;

	sigaction (SIGINT, NULL, &sa_int);
	if (sa_int.sa_handler != SIG_IGN) {
		sigaction(SIGINT, &sa_new, &sa_int);
	}
	sigaction (SIGTERM, NULL, &sa_term);
	if (sa_int.sa_handler != SIG_IGN) {
		sigaction(SIGTERM, &sa_new, &sa_term);
	}
#endif
	// force creation of the socket map
	SocketMap::getMap();
}


// Run at library unload time to close sockets left open by the app
//  sadly will not be called if the app is terminated due to a signal
void __attribute__ ((destructor)) api_destruct(void)
{
	cleanup();
}

} // extern "C"
