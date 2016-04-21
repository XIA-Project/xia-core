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
#include "state.h"

#define LIBNAME	"libc.so.6"

using namespace std;

extern "C" {

socket_t _f_socket;
bind_t _f_bind;
getsockname_t _f_getsockname;
setsockopt_t _f_setsockopt;
close_t _f_close;
fcntl_t _f_fcntl;
select_t _f_select;
poll_t _f_poll;
sendto_t _f_sendto;
recvfrom_t _f_recvfrom;
fork_t _f_fork;

size_t mtu_internal = 0;
size_t mtu_wire = 1500;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;



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
	if(!(_f_poll = (poll_t)dlsym(handle, "poll")))
		printf("can't find select!\n");
	if(!(_f_sendto = (sendto_t)dlsym(handle, "sendto")))
		printf("can't find sendto!\n");
	if(!(_f_recvfrom = (recvfrom_t)dlsym(handle, "recvfrom")))
		printf("can't find recvfrom!\n");
	if(!(_f_fork = (fork_t)dlsym(handle, "fork")))
		printf("can't find fork!\n");

    api_mtu();
    get_conf();

    // force creation of the socket map
	SocketMap::getMap();
}

// Run at library unload time to close sockets left open by the app
//  sadly will not be called if the app is terminated due to a signal
void __attribute__ ((destructor)) api_destruct(void)
{
	// loop through left open sockets and close them
	SocketMap *socketmap = SocketMap::getMap();
	SMap *sockets = socketmap->smap();
	SMap::iterator it;

	for (it = sockets->begin(); it != sockets->end(); it++) {
		Xclose(it->first);
	}
}




/*!
** @brief Specify the location of the XSockets configuration file.
**
** Specifies the name of a config file to read, and (re)loads the
** conf file.
**
** @returns void
*/
void set_conf(const char *filename, const char* sectionname)
{
	pthread_mutex_lock(&lock);

	char root[BUF_SIZE];

	snprintf(__XSocketConf::master_conf, BUF_SIZE, "%s%s", XrootDir(root, BUF_SIZE), "/etc/xsockconf.ini");
    __InitXSocket::read_conf(filename, sectionname);
	__XSocketConf::initialized=1;
	pthread_mutex_unlock(&lock);
}


} /* extern C */

/*!
** @brief Retreive a pointer to the config settings structure.
**
** Returns a pointer to the conf object containing the addresses of the API
** client's IP address and the IP addresses used for communicating with click.
** When first called, a mutex is locked and the config setting are loaded from
** file. Subsequent calls do not incur the overhead of a mutex operation.
**
** @returns pointer to the gobal XSocketConf structure
*/
struct __XSocketConf* get_conf()
{

	if (__XSocketConf::initialized == 0) {

		pthread_mutex_lock(&lock);

		if (__XSocketConf::initialized == 0) {
			__InitXSocket();
		}

		__XSocketConf::initialized=1;
		pthread_mutex_unlock(&lock);
	}
	return &_conf;
}

/*!
** @brief constructor for the Xsockets library config settings.
**
** Creates the config object and loads the settings from the config file.
**
** NOTE: document the conf file format
**
*/
__InitXSocket::__InitXSocket()
{
	const char *inifile = getenv("XSOCKCONF");

	memset(_conf.click_port, 0, __PORT_LEN);

	if (inifile==NULL) {
		inifile = "xsockconf.ini";
	}
	const char *section_name = NULL;
	char buf[PATH_MAX+1];
	int rc;

	char root[BUF_SIZE];
	snprintf(__XSocketConf::master_conf, BUF_SIZE, "%s%s", XrootDir(root, BUF_SIZE), "/etc/xsockconf.ini");

	if ((rc = readlink("/proc/self/exe", buf, sizeof(buf) - 1)) != -1) {
		buf[rc] = 0;
		section_name = basename(buf);
	}

	const char * section_name_env  = getenv("XSOCKCONF_SECTION");
	if (section_name_env)
		section_name = section_name_env;

	// NOTE: unlikely, but what happens if section_name is NULL?
	read_conf(inifile, section_name);
}

/*!
** @brief loads the specified config file and section into the global config object
**
** @warning As currently implemented, this is not thread safe if called directly.
**
** @returns void
*/
void __InitXSocket::read_conf(const char *inifile, const char *section_name)
{
	char host[256];

	if (ini_gets(section_name, "host", "", host, sizeof(host), inifile) > 0) {
		// local ini file specified a host entry in the master
		// look for the specified host entry in the master conf file, and return the default port if not found
		ini_gets(host, "click_port", DEFAULT_CLICKPORT, _conf.click_port, \
				 __PORT_LEN , __XSocketConf::master_conf);
		ini_gets(host, "cache_in_port", DEFAULT_CACHE_IN_PORT, _conf.cache_in_port, \
				 __PORT_LEN , __XSocketConf::master_conf);
		ini_gets(host, "cache_out_port", DEFAULT_CACHE_OUT_PORT, _conf.cache_out_port, \
				 __PORT_LEN , __XSocketConf::master_conf);

	} else if (ini_gets(section_name, "click_port", "", _conf.click_port, __PORT_LEN , inifile) == 0) {
		// look for a port entry under the section specified in the local ini file
		// if not found, look for that section in the master ini file
		ini_gets(section_name, "click_port", DEFAULT_CLICKPORT, _conf.click_port, \
				 __PORT_LEN , __XSocketConf::master_conf);
		ini_gets(host, "cache_in_port", DEFAULT_CACHE_IN_PORT, _conf.cache_in_port, \
				 __PORT_LEN , __XSocketConf::master_conf);
		ini_gets(host, "cache_out_port", DEFAULT_CACHE_OUT_PORT, _conf.cache_out_port, \
				 __PORT_LEN , __XSocketConf::master_conf);
	}
}

struct __XSocketConf _conf;

/*!
** @brief Print the contents of the configuration block.
**
** C helper function that calls the __InitXSocket::print_conf() routine.
**
** @returns void
*/
void print_conf()
{
	__InitXSocket::print_conf();
}

/*!
** @brief print the loaded settings for the click communications channel
**
** Prints the address of the XAPI side IP address as well as the IP addresses
** used by click for the control and data channels.
**
** @returns void
*/
void __InitXSocket::print_conf()
{
  printf("click_port %s\n", _conf.click_port);
}
int  __XSocketConf::initialized=0;
char __XSocketConf::master_conf[BUF_SIZE];

