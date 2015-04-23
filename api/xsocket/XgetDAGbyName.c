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
#include <unistd.h>
#include <syslog.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "xns.h"
#include "dagaddr.hpp"

#define ETC_HOSTS "/etc/hosts.xia"

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
	char buf[BUF_SIZE];
	FILE *hostsfp = fopen(strcat(XrootDir(buf, BUF_SIZE), ETC_HOSTS), "r");
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

// User passes a buffer and we fill it in
int XgetNamebyDAG(char *name, int namelen, const sockaddr_x *addr, socklen_t *addrlen)
{
	int sock;
	int rc;
	int result;
	sockaddr_x ns_dag;
	char pkt[NS_MAX_PACKET_SIZE];

	Graph gph(addr);
	LOGF("Looking up name for DAG:%s\n", gph.dag_string().c_str());
	if (!name) {
		LOG("ERROR: name argument was null\n");
		errno = EINVAL;
		return -1;
	}

	if (!addr || !addrlen || *addrlen < sizeof(sockaddr_x)) {
		LOG("ERROR: addr or addrlen were invalid\n");
		errno = EINVAL;
		return -1;
	}

	/* TODO: Do we need to look at hosts.xia for reverse lookup? -Nitin
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

	if (!strncmp(name, "RE ", 3) || !strncmp(name, "DAG ", 4)) {

        // check to see if name is actually a dag to begin with
        Graph gcheck(name);

        // check to see if the returned dag was valid
        // we may want a better check for this in the future
        if (gcheck.num_nodes() > 0) {
            std::string s = gcheck.dag_string();
            gcheck.fill_sockaddr((sockaddr_x*)addr);
            *addrlen = sizeof(sockaddr_x);
            return 0;
        }
    }
	*/

	// Prepare to talk to the nameserver
	if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
		return -1;

	//Read the nameserver DAG (the one that the name-query will be sent to)
	if ( XreadNameServerDAG(sock, &ns_dag) < 0 ) {
		LOG("Unable to find nameserver address");
		errno = NO_RECOVERY;
		return -1;
	}

	//Construct a name-query packet
	Graph g(addr);
	char *addrstr = strdup(g.dag_string().c_str());
	if(addrstr == NULL) {
		LOG("Unable to allocate memory to store DAG");
		errno = NO_RECOVERY;
		return -1;
	}

	LOGF("Looking for name associated with: %s\n", addrstr);

	ns_pkt query_pkt;
	query_pkt.type = NS_TYPE_RQUERY;
	query_pkt.flags = 0;
	query_pkt.name = NULL;
	query_pkt.dag = addrstr;
	int len = make_ns_packet(&query_pkt, pkt, sizeof(pkt));
	free(addrstr);

	//Send a name query to the name server
	if ((rc = Xsendto(sock, pkt, len, 0, (const struct sockaddr*)&ns_dag, sizeof(sockaddr_x))) < 0) {
		int err = errno;
		LOGF("Error sending name query (%d)", rc);
		Xclose(sock);
		errno = err;
		return -1;
	}

	//Check the response from the name server
	memset(pkt, 0, sizeof(pkt));
	if ((rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, NULL, NULL)) < 0) {
		int err = errno;
		LOGF("Error retrieving name query (%d)", rc);
		Xclose(sock);
		errno = err;
		return -1;
	}

	ns_pkt resp_pkt;
	get_ns_packet(pkt, rc, &resp_pkt);

	switch (resp_pkt.type) {
	case NS_TYPE_RESPONSE_RQUERY:
		LOG("Got valid reverse query response");
		result = 1;
		break;
	case NS_TYPE_RESPONSE_ERROR:
		LOG("Got invalid reverse query response");
		result = -1;
		break;
	default:
		LOG("Unknown nameserver response");
		result = -1;
		break;
	}
	Xclose(sock);

	if (result < 0) {
		return result;
	}

	bzero(name, namelen);
	strncpy(name, resp_pkt.name, namelen-1);
	return 0;
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
	int rc;
	int result;
	sockaddr_x ns_dag;
	char pkt[NS_MAX_PACKET_SIZE];
	char *dag;

	if (!name || *name == 0) {
		errno = EINVAL;
		return -1;
	}

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

	if (!strncmp(name, "RE ", 3) || !strncmp(name, "DAG ", 4)) {

        // check to see if name is actually a dag to begin with
        Graph gcheck(name);

        // check to see if the returned dag was valid
        // we may want a better check for this in the future
        if (gcheck.num_nodes() > 0) {
            std::string s = gcheck.dag_string();
            gcheck.fill_sockaddr((sockaddr_x*)addr);
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
	query_pkt.flags = 0;
	query_pkt.name = name;
	query_pkt.dag = NULL;
	int len = make_ns_packet(&query_pkt, pkt, sizeof(pkt));

	//Send a name query to the name server
	if ((rc = Xsendto(sock, pkt, len, 0, (const struct sockaddr*)&ns_dag, sizeof(sockaddr_x))) < 0) {
		int err = errno;
		LOGF("Error sending name query (%d)", rc);
		Xclose(sock);
		errno = err;
		return -1;
	}

	//Check the response from the name server
	memset(pkt, 0, sizeof(pkt));
	if ((rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, NULL, NULL)) < 0) {
		int err = errno;
		LOGF("Error retrieving name query (%d)", rc);
		Xclose(sock);
		errno = err;
		return -1;
	}

	ns_pkt resp_pkt;
	get_ns_packet(pkt, rc, &resp_pkt);

	switch (resp_pkt.type) {
	case NS_TYPE_RESPONSE_QUERY:
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

	if (result < 0) {
		return result;
	}

	Graph g(resp_pkt.dag);
	g.fill_sockaddr(addr);
	*addrlen = sizeof(sockaddr_x);
	return 0;
}

#define MAX_RV_DAG_SIZE 1024
#define MAX_XID_STR_SIZE 64
int XrendezvousUpdate(const char *hidstr, sockaddr_x *DAG)
{
	// Find the rendezvous service control address
	char rvControlDAG[MAX_RV_DAG_SIZE];
	if(XreadRVServerControlAddr(rvControlDAG, MAX_RV_DAG_SIZE)) {
		syslog(LOG_INFO, "No RV control address. Skipping update");
		return 1;
	}
	Graph rvg(rvControlDAG);
	sockaddr_x rv_dag;
	rvg.fill_sockaddr(&rv_dag);

	// Validate arguments
	if(!DAG) {
		syslog(LOG_ERR, "NULL DAG for rendezvous update");
		return -1;
	}
	Graph g(DAG);
	if(g.num_nodes() <= 0) {
		syslog(LOG_ERR, "Invalid DAG provided for rendezvous update");
		return -1;
	}
	std::string dag_string = g.dag_string();

	// Prepare a control message for the rendezvous service
	int controlPacketLength = MAX_XID_STR_SIZE + MAX_RV_DAG_SIZE;
	char controlPacket[controlPacketLength];
	int index = 0;
	strcpy(&controlPacket[index], hidstr);
	index += strlen(hidstr) + 1;
	strcpy(&controlPacket[index], dag_string.c_str());
	index += dag_string.size() + 1;
	controlPacketLength = index;
	// TODO: No intrinsic security for now

	// Create a socket, and send the message over it
	int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if(sock < 0) {
		syslog(LOG_ERR, "Failed creating socket to talk with RV server");
		return -1;
	}
	if(Xsendto(sock, controlPacket, controlPacketLength, 0, (const struct sockaddr *)&rv_dag, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ERR, "Failed sending registration message to RV server");
		return -1;
	}
	// TODO: Receive ack from server
	return 0;
}

/*
** called by XregisterName and XregisterHost to do the actual work
*/
int _xregister(const char *name, sockaddr_x *DAG, short flags) {
	int sock;
	int rc;
	int result;
	sockaddr_x ns_dag;
	char pkt[NS_MAX_PACKET_SIZE];

	if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
		return -1;

	//Read the nameserver DAG (the one that the name-registration will be sent to)
	if (XreadNameServerDAG(sock, &ns_dag) < 0) {
		LOG("Unable to find nameserver address");
		errno = NO_RECOVERY;
		return -1;
	}

	if (!name || *name == 0) {
		errno = EINVAL;
		return -1;
	}

	if (!DAG) {
		errno = EINVAL;
		return -1;
	}

	if (DAG->sx_family != AF_XIA) {
		errno = EINVAL;
		return -1;
	}

	Graph g(DAG);
	if (g.num_nodes() <= 0) {
		errno = EINVAL;
		return -1;
	}

	std::string dag_string = g.dag_string();

	//Construct a registration packet
	ns_pkt register_pkt;
	register_pkt.type = NS_TYPE_REGISTER;
	register_pkt.flags = flags;
	register_pkt.name = name;
	register_pkt.dag = dag_string.c_str();
	int len = make_ns_packet(&register_pkt, pkt, sizeof(pkt));

	//Send the name registration packet to the name server
	if ((rc = Xsendto(sock, pkt, len, 0, (const struct sockaddr *)&ns_dag, sizeof(sockaddr_x))) < 0) {
		int err = errno;
		LOGF("Error sending name registration (%d)", rc);
		Xclose(sock);
		errno = err;
		return -1;
	}

	fd_set read_fds;
	FD_ZERO(&read_fds);
	FD_SET(sock, &read_fds);
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if(Xselect(sock+1, &read_fds, NULL, NULL, &timeout) == 0) {
		LOGF("ERROR: Application should try again. No registration response in %d seconds", (int)timeout.tv_sec);
		return -1;
	}

	//Check the response from the name server
	memset(pkt, 0, sizeof(pkt));
	if ((rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, NULL, NULL)) < 0) {
		int err = errno;
		LOGF("Error sending name registration (%d)", rc);
		Xclose(sock);
		errno = err;
		return -1;
	}

	ns_pkt resp_pkt;
	get_ns_packet(pkt, rc, &resp_pkt);

	switch (resp_pkt.type) {
	case NS_TYPE_RESPONSE_REGISTER:
		result = 0;
		break;
	case NS_TYPE_RESPONSE_ERROR:
		result = -1;
		break;
	default:
		LOGF("Unknown NS packet type (%d)", resp_pkt.type);
		result = -1;
		break;
	 }

	Xclose(sock);
	return result;
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
	return _xregister(name, DAG, 0);
}


/*
** only used by xhcp_client to register a host name record
** Migrate flag is used to trigger updating of related name server records
**  when the host has moved to a new AD
*/
int XregisterHost(const char *name, sockaddr_x *DAG) {
	return _xregister(name, DAG, NS_FLAGS_MIGRATE);
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
	int rc;

	if (!addr || !addrlen) {
		LOG("pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (*addrlen < sizeof(sockaddr_x)) {
		errno = EINVAL;
		return -1;
	}

	int stype = getSocketType(sockfd);
	if (stype != SOCK_STREAM && stype != SOCK_DGRAM) {
		LOG("Xgetpeername is only valid with stream sockets.");
		errno = EOPNOTSUPP;
		return -1;
	}

	if (getConnState(sockfd) != CONNECTED) {
		LOGF("Socket %d is not connected", sockfd);
		errno = ENOTCONN;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETPEERNAME);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	// send the protobuf containing the user data to click
    if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// get the dag
	xsm.Clear();
	if ((rc = click_reply(sockfd, seq, &xsm)) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

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

	if (!addr || !addrlen) {
		LOG("pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (*addrlen < sizeof(sockaddr_x)) {
		errno = EINVAL;
		return -1;
	}

	if (getSocketType(sockfd) == XSOCK_INVALID)
	{
		LOG("The socket is not a valid Xsocket");
		errno = EBADF;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETSOCKNAME);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	// send the protobuf containing the user data to click
    if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// get the dag
	xsm.Clear();

	if ((rc = click_reply(sockfd, seq, &xsm)) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	if (xsm.type() != xia::XGETSOCKNAME) {
		LOGF("error: expected %d, got %d\n", xia::XGETPEERNAME, xsm.type());
		return -1;
	}

	xia::X_GetSockname_Msg *msg = xsm.mutable_x_getsockname();

	if (strcmp(msg->dag().c_str(), "RE (invalid)") == 0) {

		// socket is not initialized yet
		// FIXME: can we do a better return here?
		errno = EBADF;
		return -1;
	}

	Graph g(msg->dag().c_str());

	g.fill_sockaddr((sockaddr_x*)addr);
	*addrlen = sizeof(sockaddr_x);

	return 0;
}

int make_ns_packet(ns_pkt *np, char *pkt, int pkt_sz)
{
	char *end = pkt;

	// this had better not happen
	if (!np || !pkt || pkt_sz == 0)
		return 0;

	memset(pkt, 0, pkt_sz);
	pkt[0] = np->type;
	pkt[1] = np->flags;
	end += 2;

	switch (np->type) {
		case NS_TYPE_REGISTER:
			if (np->name == NULL || np->dag == NULL)
				return 0;
			strcpy(end, np->name);
			end += strlen(np->name) + 1;

			strcpy(end, np->dag);
			end += strlen(np->dag) + 1;
			break;

		case NS_TYPE_QUERY:
			if (np->name == NULL)
				return 0;
			strcpy(end, np->name);
			end += strlen(np->name) + 1;
			break;

		case NS_TYPE_RESPONSE_QUERY:
			if (np->dag == NULL)
				return 0;
			strcpy(end, np->dag);
			end += strlen(np->dag) + 1;
			break;

		case NS_TYPE_RQUERY:
			if (np->dag == NULL)
				return 0;
			strcpy(end, np->dag);
			end += strlen(np->dag) + 1;
			break;

		case NS_TYPE_RESPONSE_RQUERY:
			if (np->name == NULL)
				return 0;
			strcpy(end, np->name);
			end += strlen(np->name) + 1;
			break;

		default:
			break;
	}

	return end - pkt;
}

void get_ns_packet(char *pkt, int sz, ns_pkt *np)
{
	if (sz < 2) {
		// hacky error check
		np->type = NS_TYPE_RESPONSE_ERROR;
		return;
	}

	np->type  = pkt[0];
	np->flags = pkt[1];
	np->name  = np->dag = NULL;

	switch (np->type) {
		case NS_TYPE_QUERY:
			np->name = &pkt[2];
			break;

		case NS_TYPE_RQUERY:
			np->dag = &pkt[2];
			break;

		case NS_TYPE_REGISTER:
			np->name = &pkt[2];
			np->dag = np->name + strlen(np->name) + 1;
			break;

		case NS_TYPE_RESPONSE_QUERY:
			np->dag = &pkt[2];
			break;

		case NS_TYPE_RESPONSE_RQUERY:
			np->name = &pkt[2];
			break;

		default:
			break;
	}
}
