#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

int XupdateAD(int sockfd, char *newad) {
  int rc;

  if (!newad) {
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
  xsm.set_type(xia::XCHANGEAD);

  xia::X_Changead_Msg *x_changead_msg = xsm.mutable_x_changead();
  x_changead_msg->set_dag(newad);
  
  if ((rc = click_control(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  }

  return 0;
}
