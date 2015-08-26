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

int send_command(xcache_cmd *cmd)
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

#define API_CHUNKSIZE 129

int get_response_blocking(xcache_cmd *cmd)
{
	char buf[API_CHUNKSIZE] = {0};
	std::string buffer;
	int ret;

	do {
		printf("%s: Waiting\n", __func__);
		ret = read(xcache_sock, buf, API_CHUNKSIZE);
		if(ret == 0)
			break;

		std::string temp(buf, ret);

		buffer += temp;
		memset(buf, 0, API_CHUNKSIZE);
		printf("%s: Buffer blown to %lu\n", __func__, buffer.length());
		// FIXME: This is an insanely bad way of receiving data.
		// Once we have better knowledge about APIs, this is a must fix

	} while(ret == API_CHUNKSIZE);

	if(ret == 0) {
		cmd->set_cmd(xcache_cmd::XCACHE_ERROR);
		return -1;
	} else {
		cmd->ParseFromString(buffer);
	}

	return 0;
}
