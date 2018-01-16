/*
 ** Copyright 2017 Carnegie Mellon University
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
#include "RouteModule.hh"
#include <syslog.h>


RouteModule::RouteModule(const char *name)
{
	_hostname       = name;
	_broadcast_sock = -1;
	_local_sock     = -1;
	_source_sock    = -1;
	_recv_sock      = -1;
}


int RouteModule::makeSocket(Graph &g, sockaddr_x *sa)
{
	int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);

	if (sock < 0) {
		syslog(LOG_ALERT, "Unable to create socket: %s", strerror(errno));
		return -1;
	}

	g.fill_sockaddr(sa);

	if (Xbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		syslog(LOG_ALERT, "unable to bind to DAG : %s", g.dag_string().c_str());
		Xclose(sock);
		sock = -1;
	}

	return sock;
}

int RouteModule::getXIDs(std::string &ad, std::string &hid)
{
	char s[MAX_DAG_SIZE];

	int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);

	if (sock < 0) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		return -1;
	}

	// get our AD and HID
	if (XreadLocalHostAddr(sock, s, MAX_DAG_SIZE, NULL, 0) < 0 ) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		Xclose(sock);
		return -1;
	}

	Xclose(sock);

	Graph g(s);
	ad  = g.intent_AD_str();
	hid = g.intent_HID_str();

	return 0;
}


int RouteModule::makeLocalSocket()
{
	struct sockaddr_in sin;

	inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);
	sin.sin_port = htons(LOCAL_PORT);
	sin.sin_family = AF_INET;

	_local_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (_local_sock < 0) {
		syslog(LOG_ALERT, "Unable to create the local control socket");
		return -1;
	}
	if (bind(_local_sock, (struct sockaddr*)&sin, sizeof(struct sockaddr)) < 0) {
		syslog(LOG_ALERT, "unable to bind to localhost:%u", LOCAL_PORT);
		return -1;
	}

	return 0;
}


int RouteModule::readMessage(char *recv_message, struct pollfd *pfd, unsigned npfds, int *iface, bool *local)
{
	int rc = -1;
	sockaddr_x theirDAG;
	struct timespec tspec;

	tspec.tv_sec = 0;
	tspec.tv_nsec = 500000000;

	rc = Xppoll(pfd, npfds, &tspec, NULL);
	if (rc > 0) {
		int sock = -1;

		for (unsigned i = 0; i < npfds; i++) {
			if (!(pfd[i].revents & POLLIN)) {
				continue;
			}

			sock = pfd[i].fd;

			printf("sock = %d\n", sock);
			if (local) {
				*local = (sock == _local_sock);
			}

			memset(&recv_message[0], 0, BUFFER_SIZE);

			if (sock <= 0) {
				// something weird happened
				return 0;

			} else if (sock == _local_sock) {
				*iface = FALLBACK;
				socklen_t sz = sizeof(sockaddr_in);

				if ((rc = recvfrom(sock, recv_message, BUFFER_SIZE, 0, (sockaddr*)&_local_sa, &sz)) < 0) {
					syslog(LOG_WARNING, "local message receive error");
				}
			} else {
				struct msghdr mh;
				struct iovec iov;
				struct in_pktinfo pi;
				struct cmsghdr *cmsg;
				struct in_pktinfo *pinfo;
				char cbuf[CMSG_SPACE(sizeof pi)];

				iov.iov_base = recv_message;
				iov.iov_len = BUFFER_SIZE;

				mh.msg_name = &theirDAG;
				mh.msg_namelen = sizeof(theirDAG);
				mh.msg_iov = &iov;
				mh.msg_iovlen = 1;
				mh.msg_control = cbuf;
				mh.msg_controllen = sizeof(cbuf);

				cmsg = CMSG_FIRSTHDR(&mh);
				cmsg->cmsg_level = IPPROTO_IP;
				cmsg->cmsg_type = IP_PKTINFO;
				cmsg->cmsg_len = CMSG_LEN(sizeof(pi));

				mh.msg_controllen = cmsg->cmsg_len;

				if ((rc = Xrecvmsg(sock, &mh, 0)) < 0) {
					perror("recvfrom");

				} else {
					for (cmsg = CMSG_FIRSTHDR(&mh); cmsg != NULL; cmsg = CMSG_NXTHDR(&mh, cmsg)) {
						if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
							pinfo = (struct in_pktinfo*) CMSG_DATA(cmsg);
							*iface = pinfo->ipi_ifindex;
						}
					}
				}
			}
		}
	}
	return rc;
}

int RouteModule::sendMessage(int sock, sockaddr_x *dest, const Xroute::XrouteMsg &msg)
{
	int rc;
	string message;
	msg.SerializeToString(&message);

	rc = Xsendto(sock, message.c_str(), message.length(), 0, (sockaddr*)dest, sizeof(sockaddr_x));
	if (rc < 0) {
		syslog(LOG_WARNING, "unable to send %s msg: %s", Xroute::msg_type_Name(msg.type()).c_str(), strerror(errno));
	}
	return rc;
}

int RouteModule::sendMessage(sockaddr_x *dest, const Xroute::XrouteMsg &msg)
{
	return sendMessage(_source_sock, dest, msg);
}

int RouteModule::sendMessage(sockaddr *dest, const Xroute::XrouteMsg &msg)
{
	int rc;
	string message;
	msg.SerializeToString(&message);

	rc = sendto(_source_sock, message.c_str(), message.length(), 0, dest, sizeof(sockaddr));
	if (rc < 0) {
		syslog(LOG_WARNING, "unable to send %s msg: %s", Xroute::msg_type_Name(msg.type()).c_str(), strerror(errno));
	}
	return rc;
}

int RouteModule::run()
{
	int rc = 0;

	// connect to the click route engine
	_xr.setRouter(_hostname);
	if ((rc = _xr.connect()) != 0) {
		syslog(LOG_ALERT, "unable to connect to click (%d)", rc);
		exit(-1);
	}

	makeLocalSocket();

	init();
	_enabled = true;

	while (_enabled && rc >= 0) {
		rc = handler();
	}
	return rc;
}

