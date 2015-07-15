#ifndef __XCACHE_PRIV_H__
#define __XCACHE_PRIV_H__

#include "../proto/xcache_cmd.pb.h"

extern int xcache_sock;

int send_command(xcache_cmd *cmd);
int get_response_blocking(xcache_cmd *cmd);

#endif
