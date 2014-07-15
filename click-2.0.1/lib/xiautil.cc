// -*- related-file-name: "../include/click/xiautil.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiautil.hh>
#if CLICK_USERLEVEL
#include <stdlib.h>
#include <unistd.h>
#endif
CLICK_DECLS

char *xiaRootDir(char *buf, unsigned len) {
	char *dir;
	char *pos;

	if (buf == NULL || len == 0)
		return NULL;

	if ((dir = getenv("XIADIR")) != NULL) {
		strncpy(buf, dir, len);
		return buf;
	}
#ifdef __APPLE__
	if (!getcwd(buf, len)) {
		buf[0] = 0;
		return buf;
	}
#else
	int cnt = readlink("/proc/self/exe", buf, len);

	if (cnt < 0) {
		buf[0] = 0;
		return buf;
	}
	else if ((unsigned)cnt == len)
		buf[len - 1] = 0;
	else
		buf[cnt] = 0;
#endif
	pos = strstr(buf, SOURCE_DIR);
	if (pos) {
		pos += sizeof(SOURCE_DIR) - 1;
		*pos = '\0';
	}
	return buf;
}

CLICK_ENDDECLS
