#ifndef __XCACHE_PRIV_H__
#define __XCACHE_PRIV_H__

#include "../proto/xcache_cmd.pb.h"

extern int xcache_sock;

int send_command(XcacheCommand *cmd);
int get_response_blocking(XcacheCommand *cmd);

#endif
