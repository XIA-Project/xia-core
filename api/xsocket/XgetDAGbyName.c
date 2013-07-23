/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**	http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
 @file XgetDAGbyName.c
 @brief Implements XgetDAGbyName(), XregisterName(), Xgetpeername() and Xgetsockname()
*/
#include <errno.h>
#include <fcntl.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"

#define SOURCE_DIR "xia-core"
#define ETC_HOSTS "/etc/hosts.xia"

#define NS_MAX_PACKET_SIZE 1024
#define NS_MAX_DAG_LENGTH 1024

#define NS_TYPE_REGISTER 1
#define NS_TYPE_QUERY 2
#define NS_TYPE_RESPONSE 3
#define NS_TYPE_RESPONSE_ERROR 4

typedef struct ns_pkt {
	short type;
	char* name;
	char* dag;
} ns_pkt;

#define BUF_SIZE 4096

/*!
** @brief Finds the root of the source tree
**
** @returns a character pointer to the root of the source tree
**
*/
char *findRoot() {
	char *pos;
	char *path = (char*)malloc(sizeof(char) * BUF_SIZE);
	int len = readlink("/proc/self/exe", path, BUF_SIZE);

	if (len < 0)
		return NULL;
	else if (len == BUF_SIZE)
		path[BUF_SIZE - 1] = 0;
	else
		path[len] = 0;

	pos = strstr(path, SOURCE_DIR);
	if(pos) {
		pos += sizeof(SOURCE_DIR)-1;
		*pos = '\0';
	}
	return path;
}

/*!
** @brief Lookup a DAG in the hosts.xia file
**
** @param name The name of an XIA service or host.
**
** @returns a character point to the dag on success
** @returns NULL on failure
**
*/
char *hostsLookup(const char *name) {
	char line[512];
	char *linend;
	char *dag;
	char _dag[NS_MAX_DAG_LENGTH];

	// look for an hosts_xia file locally
	FILE *hostsfp = fopen(strcat(findRoot(), ETC_HOSTS), "r");
	int answer_found = 0;
	if (hostsfp != NULL) {
		while (fgets(line, 511, hostsfp) != NULL) {
			linend = line+strlen(line)-1;
			while (*linend == '\r' || *linend == '\n' || *linend == '\0') {
				linend--;
			}
			*(linend+1) = '\0';
			if (line[0] == '#') {
				continue;
			} else if (!strncmp(line, name, strlen(name))
						&& line[strlen(name)] == ' ') {
				strncpy(_dag, line+strlen(name)+1, strlen(line)-strlen(name)-1);
				_dag[strlen(line)-strlen(name)-1] = '\0';
				answer_found = 1;
			}
		}
		fclose(hostsfp);
		if (answer_found) {
			dag = (char*)malloc(sizeof(_dag) + 1);
			strcpy(dag, _dag);
			return dag;
		}
	} else {
		//printf("XIAResolver file error\n");
	}

	//printf("Name not found in ./hosts_xia\n");
	return NULL;
}


/*!
** @brief Lookup a DAG based using a host or service name.
**
** The name should be a string such as www_s.example.xia or host.example.xia.
** By convention services are indicated by '_s' appended to the service name.
** The memory returned is dynamically allocated and should be released with a
** call to free() when the caller is done with it.
**
** This is a very simple implementation of the name query function.
** It will be replaces in a future release.
**
** @param name The name of an XIA service or host.
**
** @returns a character point to the dag on success
** @returns NULL on failure
**
*/
int XgetDAGbyName(const char *name, sockaddr_x *addr, socklen_t *addrlen)
{
	int sock;
	sockaddr_x ns_dag;
	char pkt[NS_MAX_PACKET_SIZE];
	char *dag;
	char _name[NS_MAX_DAG_LENGTH], _dag[NS_MAX_DAG_LENGTH];
	int result;

	if (!addr || !addrlen || *addrlen < sizeof(sockaddr_x)) {
		errno = EINVAL;
		return -1;
	}

	// see if name is registered in the local hosts.xia file
	if((dag = hostsLookup(name))) {

		Graph g(dag);
		free(dag);

		// check to see if the returned dag was valid
		// we may want a better check for this in the future
		if (g.num_nodes() > 0) {
			std::string s = g.dag_string();
			g.fill_sockaddr((sockaddr_x*)addr);
			*addrlen = sizeof(sockaddr_x);
			return 0;
		}
	}

	// not found locally, check the name server
	if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
		return -1;

	//Read the nameserver DAG (the one that the name-query will be sent to)
	if ( XreadNameServerDAG(sock, &ns_dag) < 0 ) {
		LOG("Unable to find nameserver address");
		errno = NO_RECOVERY;
		return -1;
	}

	//Construct a name-query packet
	ns_pkt query_pkt;
	query_pkt.type = NS_TYPE_QUERY;
	query_pkt.name = strdup(name);

	memset(pkt, 0, sizeof(pkt));
	int offset = 0;
	memcpy(pkt, &query_pkt.type, sizeof(query_pkt.type));
	offset += sizeof(query_pkt.type);
	memcpy(pkt+offset, query_pkt.name, strlen(query_pkt.name)+1);
	offset += strlen(query_pkt.name)+1;

	//Send a name query to the name server
	Xsendto(sock, pkt, offset, 0, (const struct sockaddr*)&ns_dag, sizeof(sockaddr_x));

	//Check the response from the name server
	memset(pkt, 0, sizeof(pkt));
	int rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, NULL, NULL);
	if (rc < 0) { perror("recvfrom"); }

	memset(_name, '\0', NS_MAX_DAG_LENGTH);
	memset(_dag, '\0', NS_MAX_DAG_LENGTH);

	ns_pkt *tmp = (ns_pkt *)pkt;
	char* tmp_name = (char*)(pkt+sizeof(tmp->type));
	char* tmp_dag = (char*)(pkt+sizeof(tmp->type)+ strlen(tmp_name)+1);
	switch (tmp->type) {
	case NS_TYPE_RESPONSE:
		sprintf(_name, "%s", tmp_name);
		sprintf(_dag, "%s", tmp_dag);
		result = 1;
		break;
	case NS_TYPE_RESPONSE_ERROR:
		result = -1;
		break;
	default:
		LOG("Unknown nameserver response");
		result = -1;
		break;
	}

	Xclose(sock);
	free(query_pkt.name);

	if (result < 0) {
		return result;
	}

	Graph g(_dag);
	g.fill_sockaddr(addr);

	return 0;
}


/*!
** @brief Register a service or hostname with the name server.
**
** Register a host or service name with the XIA nameserver.
** By convention services are indicated by '_s' appended to the service name.
** The memory returned is dynamically allocated and should be released with a
** call to free() when the caller is done with it.
**
** This is a very simple implementation and will be replaced in a
** future release. This version does not check correctness of the name or dag,
** nor does it check to ensure that the client is allowed to bind to name.
**
** @param name - The name of an XIA service or host.
** @param DAG  - the DAG to be bound to name.
**
** @returns 0 on success
** @returns -1 on failure with errno set
**
*/
int XregisterName(const char *name, sockaddr_x *DAG) {
	int sock;
	sockaddr_x ns_dag;
	char pkt[NS_MAX_PACKET_SIZE];
	char _name[NS_MAX_DAG_LENGTH], _dag[NS_MAX_DAG_LENGTH];
	int result;

	if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
		return -1;

	//Read the nameserver DAG (the one that the name-registration will be sent to)
	if (XreadNameServerDAG(sock, &ns_dag) < 0) {
		LOG("Unable to find nameserver address");
		errno = NO_RECOVERY;
		return -1;
	}

	if (!DAG) {
		errno = EINVAL;
		return -1;
	}

	Graph g(DAG);
	if (g.num_nodes() <= 0) {
		errno = EINVAL;
		return -1;
	}

	//Construct a registration packet
	ns_pkt register_pkt;
	register_pkt.type = NS_TYPE_REGISTER;
	register_pkt.name = strdup(name);
	register_pkt.dag = strdup(g.dag_string().c_str());

	memset(pkt, 0, sizeof(pkt));
	int offset = 0;
	memcpy(pkt, &register_pkt.type, sizeof(register_pkt.type));
	offset += sizeof(register_pkt.type);
	memcpy(pkt+offset, register_pkt.name, strlen(register_pkt.name)+1);
	offset += strlen(register_pkt.name)+1;
	memcpy(pkt+offset, register_pkt.dag, strlen(register_pkt.dag)+1);
	offset += strlen(register_pkt.dag)+1;

	//Send the name registration packet to the name server
	//FIXME: use sockaddr here
	Xsendto(sock, pkt, offset, 0, (const struct sockaddr *)&ns_dag, sizeof(sockaddr_x));

	//Check the response from the name server
	memset(pkt, 0, sizeof(pkt));
	int rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, NULL, NULL);
	if (rc < 0) { perror("recvfrom"); }

	memset(_name, '\0', NS_MAX_DAG_LENGTH);
	memset(_dag, '\0', NS_MAX_DAG_LENGTH);

	ns_pkt *tmp = (ns_pkt *)pkt;
	char* tmp_name = (char*)(pkt+sizeof(tmp->type));
	char* tmp_dag = (char*)(pkt+sizeof(tmp->type)+ strlen(tmp_name)+1);
	switch (tmp->type) {
	case NS_TYPE_RESPONSE:
		sprintf(_name, "%s", tmp_name);
		sprintf(_dag, "%s", tmp_dag);
		result = 0;
		break;
	case NS_TYPE_RESPONSE_ERROR:
		result = -1;
		break;
	default:
		fprintf(stderr, "dafault\n");
		result = -1;
		break;
	 }
	free(register_pkt.name);
	free(register_pkt.dag);

	Xclose(sock);
	return result;
}

/*!
** @brief Get the full DAG of the remote socket.
**
** @param sockfd An Xsocket of type SOCK_STREAM
** @param dag A sockaddr to hold the returned DAG.
** @param len On input contans the size of the sockaddr
**  on output contains sizeof(sockaddr_x).
**
** @returns 0 on success
** @returns -1 on failure with errno set
** @returns errno = EFAULT if dag is NULL
** @returns errno = EOPNOTSUPP if sockfd is not of type XSSOCK_STREAM
** @returns errno = ENOTCONN if sockfd is not in a connected state
**
*/
int Xgetpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int flags;
	int rc;
	char buf[MAXBUFLEN];

	if (!addr || !addrlen) {
		LOG("pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (*addrlen < sizeof(sockaddr_x)) {
		errno = EINVAL;
		return -1;
	}

	if (validateSocket(sockfd, XSOCK_STREAM, EOPNOTSUPP) < 0) {
		LOG("Xgetpeername is only valid with stream sockets.");
		return -1;
	}

	if (connState(sockfd) != CONNECTED) {
		LOGF("Socket %d is not connected", sockfd);
		errno = ENOTCONN;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETPEERNAME);

	flags = fcntl(sockfd, F_GETFL);
	fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

	// send the protobuf containing the user data to click
    if ((rc = click_send(sockfd, &xsm)) < 0) {
		fcntl(sockfd, F_SETFL, flags);
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// get the dag
	if ((rc = click_reply(sockfd, xia::XGETPEERNAME, buf, sizeof(buf))) < 0) {
		fcntl(sockfd, F_SETFL, flags);
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	fcntl(sockfd, F_SETFL, flags);

	xsm.Clear();
	xsm.ParseFromString(buf);

	if (xsm.type() != xia::XGETPEERNAME) {
		LOGF("error: expected %d, got %d\n", xia::XGETPEERNAME, xsm.type());
		return -1;
	}

	xia::X_GetPeername_Msg *msg = xsm.mutable_x_getpeername();

	Graph g(msg->dag().c_str());

	g.fill_sockaddr((sockaddr_x*)addr);
	*addrlen = sizeof(sockaddr_x);

	return 0;
}

/*!
** @brief Get the full DAG of the local socket.
**
** @param sockfd An Xsocket of type SOCK_STREAM
** @param dag A sockaddr to hold the returned DAG.
** @param len On input contans the size of the sockaddr,
**  on output contains sizeof(sockaddr_x).
**
** @returns 0 on success
** @returns -1 on failure with errno set
** @returns errno = EFAULT if dag is NULL
** @returns errno = EOPNOTSUPP if sockfd is not of type XSSOCK_STREAM
** @returns errno = ENOTCONN if sockfd is not in a connected state
**
*/
int Xgetsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int rc;
	int flags;
	char buf[MAXBUFLEN];

	if (!addr || !addrlen) {
		LOG("pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (*addrlen < sizeof(sockaddr_x)) {
		errno = EINVAL;
		return -1;
	}

	if (validateSocket(sockfd, XSOCK_STREAM, EOPNOTSUPP) < 0) {
		LOG("Xgetsockname is only valid with stream sockets.");
		return -1;
	}

	if (connState(sockfd) != CONNECTED) {
		LOGF("Socket %d is not connected", sockfd);
		errno = ENOTCONN;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETSOCKNAME);

	flags = Xfcntl(sockfd, F_GETFL);
	fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

	// send the protobuf containing the user data to click
    if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		fcntl(sockfd, F_SETFL, flags);
		return -1;
	}

	// get the dag
	// FIXME: loop here till done or error
	if ((rc = click_reply(sockfd, xia::XGETSOCKNAME, buf, sizeof(buf))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		fcntl(sockfd, F_SETFL, flags);
		return -1;
	}

	fcntl(sockfd, F_SETFL, flags);

	xsm.Clear();
	xsm.ParseFromString(buf);

	if (xsm.type() != xia::XGETSOCKNAME) {
		LOGF("error: expected %d, got %d\n", xia::XGETPEERNAME, xsm.type());
		return -1;
	}

	xia::X_GetSockname_Msg *msg = xsm.mutable_x_getsockname();

	Graph g(msg->dag().c_str());

	g.fill_sockaddr((sockaddr_x*)addr);
	*addrlen = sizeof(sockaddr_x);

	return 0;
}
