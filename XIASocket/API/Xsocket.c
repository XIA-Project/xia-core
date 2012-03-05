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
 @file Xsocket.c
 @brief Implements the Xsocket API call.
*/

#include <sys/types.h>
#include <unistd.h>
#include <linux/unistd.h>

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <stdlib.h>
#include "minIni.h"
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>

using namespace std;

extern "C" {

	/*!
	** @brief Create an XIA socket.
	**
	** Creates an XIA socket. Must be the first Xsocket function 
	**	called.
	**
	** When called it creates a socket to connect to Click using 
	** UDP packets using a random local port. It then sends an open 
	** request to Click, from that socket. 
	** It waits (blocking) for a reply from Click. The control info 
	** is encoded in the google protobuffer message (encapsulated 
	** within UDP message).
	**
	** @param transport_type Valid values are: 
	**	\n XSOCK_STREAM for reliable communications (SID)
	**	\n XSOCK_DGRAM for a ligher weight connection, but with 
	**	unguranteed delivery (SID)
	**	\n XSOCK_CHUNK for getting/putting content chunks (CID)
	**	\n XSOCK_RAW for a raw socket that can have direct edits made to the header
	**
	** @returns socket id on success. 
	** @returns -1 on failure with errno set.
	**
	** @warning In the current implementation, the returned socket is 
	**	a normal UDP socket that is used to communicate with the click
	**	transport layer. Using this socket with normal unix socket
	**	calls will cause unexpected behaviors. Attempting to pass
	**	a socket created with the the normal socket function to the
	** Xsocket API will have similar results.
	**
	*/
	int Xsocket(int transport_type)
	{
		struct sockaddr_in addr;
		xia::XSocketCallType type;
		int rc;
		int sockfd;

		switch (transport_type) {
			case XSOCK_STREAM:
			case XSOCK_DGRAM:
			case XSOCK_CHUNK:
			case XSOCK_RAW:
				break;
			default:
				// invalid socket type requested
				errno = EAFNOSUPPORT;
				return -1;
		}

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
			LOGF("error creating Xsocket: %s", strerror(errno));
			return -1;
		}

		// bind to an unused random port number
		addr.sin_family = PF_INET;
		addr.sin_addr.s_addr = inet_addr(MYADDRESS);
		addr.sin_port = 0;

		if (bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
			close(sockfd);
			LOGF("bind error: %s", strerror(errno));
			return -1;
		}
			
		// protobuf message
		xia::XSocketMsg xsm;
		xsm.set_type(xia::XSOCKET);
			
		xia::X_Socket_Msg *x_socket_msg = xsm.mutable_x_socket();
		x_socket_msg->set_type(transport_type);		
		
		if ((rc = click_control(sockfd, &xsm)) < 0) {
			LOGF("Error talking to Click: %s", strerror(errno));
			close(sockfd);
			return -1;
		}

		// process the reply from click
		if ((rc = click_reply2(sockfd, &type)) < 0) {
			LOGF("Error getting status from Click: %s", strerror(errno));

		} else if (type != xia::XSOCKET) {
			// something bad happened
			LOGF("Expected type %d, got %d", xia::XSOCKET, type);
			errno = ECLICKCONTROL;
			rc = -1;
		}

		if (rc == 0) {
			allocSocketState(sockfd, transport_type);
			return sockfd;
		}

		// close the control socket since the underlying Xsocket is no good
		close(sockfd);
		return -1; 
    }
	
			
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	
	/*!
	** @brief Specify the location of the XSockets configuration file.
	**
	** Specifies the name of a config file to read, and (re)loads the
	** conf file.
	**
	** @warning Can we delete this function? It is not called currently
	** and has thread safety issues, not to mention the problems that 
	** changing the config settings at runtime can cause.
	**
	** @returns void
	*/
    void set_conf(const char *filename, const char* sectionname)
    {
		pthread_mutex_lock(&lock);
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
	const char * inifile = getenv("XSOCKCONF");

	memset(_conf.api_addr, 0, __IP_ADDR_LEN);
	memset(_conf.click_dataaddr, 0, __IP_ADDR_LEN);
	memset(_conf.click_controladdr, 0, __IP_ADDR_LEN);

	if (inifile==NULL) {
		inifile = "xsockconf.ini";
	}
	const char *section_name = NULL;
	char buf[PATH_MAX+1];
	int rc;

	if ((rc = readlink("/proc/self/exe", buf, sizeof(buf) - 1)) != -1) {
		section_name = basename(buf);
		buf[rc] = 0;
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
	ini_gets(section_name, "api_addr", DEFAULT_MYADDRESS, _conf.api_addr, __IP_ADDR_LEN, inifile);
	ini_gets(section_name, "click_dataaddr", DEFAULT_CLICKDATAADDRESS, _conf.click_dataaddr, __IP_ADDR_LEN , inifile);
	ini_gets(section_name, "click_controladdr", DEFAULT_CLICKCONTROLADDRESS, _conf.click_controladdr, __IP_ADDR_LEN, inifile);

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
	printf("api_addr %s\n", _conf.api_addr);
	printf("click_controladdr %s\n",  _conf.click_controladdr);
	printf("click_dataaddr %s\n",  _conf.click_dataaddr);
}
int  __XSocketConf::initialized=0;

