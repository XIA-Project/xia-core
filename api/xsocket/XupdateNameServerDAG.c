#include <errno.h>
#include <fcntl.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"

int XupdateNameServerDAG(int sockfd, char *nsDAG) {
  int rc;

  if (!nsDAG) {
    LOG("new ad is NULL!");
    errno = EFAULT;
    return -1;
  }

  if (getSocketType(sockfd) == XSOCK_INVALID) {
    LOG("The socket is not a valid Xsocket");
    errno = EBADF;
    return -1;
  }

  xia::XSocketMsg xsm;
  xsm.set_type(xia::XUPDATENAMESERVERDAG);

  xia::X_Updatenameserverdag_Msg *x_updatenameserverdag_msg = xsm.mutable_x_updatenameserverdag();
  x_updatenameserverdag_msg->set_dag(nsDAG);

  if ((rc = click_send(sockfd, &xsm)) < 0) {
	if (!WOULDBLOCK()) {
		LOGF("Error talking to Click: %s", strerror(errno));
	}
	return -1;
  }

  return 0;
}


int XreadNameServerDAG(int sockfd, sockaddr_x *nsDAG) {
	int rc = -1;
	char UDPbuf[MAXBUFLEN];

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		LOG("The socket is not a valid Xsocket");
		errno = EBADF;
		return -1;
	}

	if (!nsDAG) {
		errno = EINVAL;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XREADNAMESERVERDAG);

	int flags = fcntl(sockfd, F_GETFL);
	fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		fcntl(sockfd, F_SETFL, flags);
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	if ((rc = click_reply(sockfd, xia::XREADNAMESERVERDAG, UDPbuf, sizeof(UDPbuf))) < 0) {
		fcntl(sockfd, F_SETFL, flags);
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	fcntl(sockfd, F_SETFL, flags);

	xia::XSocketMsg xsm1;
	xsm1.ParseFromString(UDPbuf);
	if (xsm1.type() == xia::XREADNAMESERVERDAG) {
		xia::X_ReadNameServerDag_Msg *_msg = xsm1.mutable_x_readnameserverdag();

		Graph g(_msg->dag().c_str());
		if (g.num_nodes() > 0) {
			g.fill_sockaddr(nsDAG);
			rc = 0;
		}
	}
	return rc;
}

