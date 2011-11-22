/* 
 * Creates an XIA socket. 
 * When called it creates a socket to connect to Click using UDP packets. 
 * It first opens a socket and listens on some random port. Nextit sends
 * an open request to Click, from that socket. It waits (blocking) for a 
 * reply from Click. The control info is encoded in the google protobuffer message (encapsulated within UDP message).
 *
 * On success the return packet's UDP source port is the same as the 
 * destination port. On failure, source port is 0
 * 
 * Arguments:
 * char* sDAG : This is registered to allow reverse traffic
 * TODO: Add XID type
 *
 * Return values:
 * sockfd if successful
 * -1 on failure
 *
 */

#include "Xsocket.h"
#include "Xinit.h"
#include <stdlib.h>
#include "minIni.h"
#include <libgen.h>
#include <limits.h>

using namespace std;

extern "C" {

        void error(const char *msg)
        {
			perror(msg);
			exit(0);
        }

        int Xsocket()
        {
			//Setup to listen for control info
			//char* str=(char*)"open";//TODO: Not necessary. Maybe more useful data could be sent in the open control packet?
			struct sockaddr_in my_addr, their_addr;
			int rv;
			int numbytes;
			char buf[MAXBUFLEN];
			socklen_t addr_len;
			//char s[INET6_ADDRSTRLEN];
			int sockfd,tries;

			// protobuf message
			xia::XSocketMsg xia_socket_msg;

			int port;

			if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
				perror("Xsocket listener: socket");
				return -1;
			}

			//If port is in use, try next port until success, or max ATTEMPTS reached
			srand(time(NULL));
			rv=-1;
			for (tries=0;tries<ATTEMPTS;tries++)
			{
				port=1024 + rand() % (65535 - 1024);
				my_addr.sin_family = PF_INET;
				my_addr.sin_addr.s_addr = inet_addr(MYADDRESS);
				my_addr.sin_port = htons(port);
				rv=bind(sockfd, (const struct sockaddr *)&my_addr, sizeof(my_addr));
				if (rv != -1)
					break;
			}

			if (rv == -1) {
				close(sockfd);
				perror("Xsocket listener: bind");
				return -1;
			}

			//printf("Xsocket listener: Sending...\n");

			//Send a control packet
			their_addr.sin_family = PF_INET;
			their_addr.sin_addr.s_addr = inet_addr(CLICKCONTROLADDRESS);
			their_addr.sin_port = htons(atoi(CLICKCONTROLPORT));

			// protobuf message
			xia_socket_msg.set_type(xia::XSOCKET);
			std::string p_buf;
			xia_socket_msg.SerializeToString(&p_buf);

			if ((numbytes = sendto(sockfd, p_buf.c_str(), p_buf.size(), 0,
							(const struct sockaddr *)&their_addr, sizeof(their_addr))) == -1) {
				perror("Xsocket(): sendto failed");
				return(-1);
			}


			//Process the reply
			addr_len = sizeof their_addr;
			if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
							(struct sockaddr *)&their_addr, &addr_len)) == -1) {
				perror("Xsocket: recvfrom");
				return -1;
			}
			//buf[numbytes] = '\0';

			//protobuf message parsing
			xia_socket_msg.ParseFromString(buf);

			if (xia_socket_msg.type() == xia::XSOCKET) {
				return sockfd;
			}

			close(sockfd);
			return -1; 

    }
	//void *__dso_handle = NULL;
			
    void set_conf(const char *filename, const char* sectionname)
    {
        __InitXSocket::read_conf(filename, sectionname);
    }

} /* extern C */

struct __XSocketConf* get_conf() 
{
	if (__XSocketConf::initialized==0) {
		__InitXSocket();
	}
	return &_conf;
}

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

	if (readlink("/proc/self/exe", buf, sizeof(buf)-1)!=-1) {
		section_name = basename(buf);
	}

	const char * section_name_env  = getenv("XSOCKCONF_SECTION");
	if (section_name_env) 
		section_name = section_name_env;

	read_conf(inifile, section_name);
}

void __InitXSocket::read_conf(const char *inifile, const char *section_name) 
{
	__XSocketConf::initialized=1;

	ini_gets(section_name, "api_addr", DEFAULT_MYADDRESS, _conf.api_addr, __IP_ADDR_LEN, inifile);
	ini_gets(section_name, "click_dataaddr", DEFAULT_CLICKDATAADDRESS, _conf.click_dataaddr, __IP_ADDR_LEN , inifile);
	ini_gets(section_name, "click_controladdr", DEFAULT_CLICKCONTROLADDRESS, _conf.click_controladdr, __IP_ADDR_LEN, inifile);

}

struct __XSocketConf _conf;

void print_conf()
{
	__InitXSocket::print_conf();
}


void __InitXSocket::print_conf() 
{
	printf("api_addr %s\n", _conf.api_addr);
	printf("click_controladdr %s\n",  _conf.click_controladdr);
	printf("click_dataaddr %s\n",  _conf.click_dataaddr);
}
int  __XSocketConf::initialized=0;

