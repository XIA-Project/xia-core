#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

int XreadLocalHostAddr(int sockfd, char *localhostAD, char *localhostHID) {
  	int rc;
  	char UDPbuf[MAXBUFLEN];
  	
 	if (getSocketType(sockfd) == XSOCK_INVALID) {
   	 	LOG("The socket is not a valid Xsocket");
   	 	errno = EBADF;
  		return -1;
 	}

 	xia::XSocketMsg xsm;
  	xsm.set_type(xia::XREADLOCALHOSTADDR);
  
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
	if (xsm1.type() == xia::XREADLOCALHOSTADDR) {
		xia::X_ReadLocalHostAddr_Msg *_msg = xsm1.mutable_x_readlocalhostaddr();
		strcpy(localhostAD, (_msg->ad()).c_str() );
		strcpy(localhostHID, (_msg->hid()).c_str() );
	} else {
		rc = -1;
	}	
	return rc;
	 
}
