#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "xcache.h"
#include "xcachePriv.h"

int xcache_sock;

int send_command(XcacheCommand *cmd)
{
	int ret;
	int remaining, sent;

	std::string cmd_on_wire;

	cmd->SerializeToString(&cmd_on_wire);

	remaining = cmd_on_wire.length();
	sent = 0;
	do {
		int to_send;

		to_send = remaining < 512 ? remaining : 512;
		ret = send(xcache_sock, cmd_on_wire.c_str() + sent, to_send, 0);

		remaining -= to_send;
		sent += to_send;
	} while(remaining > 0);

	printf("%s: Lib sent %d bytes\n", __func__, ret);

	return ret;
}

int get_response_blocking(XcacheCommand *cmd)
{
	char buf[512] = "";
	std::string buffer;
	int ret;

	do {
		ret = read(xcache_sock, buf, 512);
		if(ret == 0)
			break;

		buffer.append(buf);
	} while(ret == 512);

	if(ret == 0) {
		cmd->set_cmd(XcacheCommand::XCACHE_ERROR);
		return -1;
	} else {
		cmd->ParseFromString(buffer);
	}

	return 0;
}
