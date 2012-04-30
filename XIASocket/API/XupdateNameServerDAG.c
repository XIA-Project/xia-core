#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

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
  
  if ((rc = click_control(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  }

  return 0;
}


int XreadNameServerDAG(int sockfd, char *nsDAG) {
  	int rc;
  	char UDPbuf[MAXBUFLEN];
  	
 	if (getSocketType(sockfd) == XSOCK_INVALID) {
   	 	LOG("The socket is not a valid Xsocket");
   	 	errno = EBADF;
  		return -1;
 	}

 	xia::XSocketMsg xsm;
  	xsm.set_type(xia::XREADNAMESERVERDAG);
  
  	if ((rc = click_control(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  	}

	if ((rc = click_reply(sockfd, UDPbuf, sizeof(UDPbuf))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	xia::XSocketMsg xsm1;
	xsm1.ParseFromString(UDPbuf);
	if (xsm1.type() == xia::XREADNAMESERVERDAG) {
		xia::X_ReadNameServerDag_Msg *_msg = xsm1.mutable_x_readnameserverdag();
		strcpy(nsDAG, (_msg->dag()).c_str() );
	} else {
		rc = -1;
	}	
	return rc;
	 
}


