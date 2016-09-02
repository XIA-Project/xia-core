/*
** Copyright 2016 Carnegie Mellon University
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <openssl/sha.h>
#include "Xsocket.h"
#include "xcache.h"

#define CACHEDIR "/tmp/content/"

int verbose = 1;

void help(const char *name)
{
	printf("\nusage: %s [-q] \n", name);
	printf("where:\n");
	printf(" -q : quiet mode\n\n");
	exit(0);
}

void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

void die(int ecode, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "aborting\n");
	exit(ecode);
}

char *hex_str(char *s, unsigned char *data, int len)
{
	char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
			 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	for (int i = 0; i < len; ++i) {
		s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
		s[2 * i + 1] = hexmap[data[i] & 0x0F];
	}
	s[SHA_DIGEST_LENGTH * 2] = 0;
	return s;
}

char *compute_cid(char *cid, const char *data, size_t len)
{
	unsigned char digest[SHA_DIGEST_LENGTH];

	SHA1((unsigned char *)data, len, digest);

	return hex_str(cid, digest, SHA_DIGEST_LENGTH);
}

void populate()
{
	DIR *cd = opendir(CACHEDIR);
	struct dirent* chunk;
	struct stat info;
	char path[256];
	char cid[64];
	char *buf;
	sockaddr_x addr;
	XcacheHandle xcache;

	XcacheHandleInit(&xcache);

	if (cd == NULL) {
		die(-1, "error: %s\n", strerror(errno));
	}

	while((chunk = readdir(cd)) != NULL) {
		sprintf(path, "%s%s", CACHEDIR, chunk->d_name);

		if (stat(path, &info) < 0) {
			die(-1, "error: %s\n", strerror(errno));
		}

		if (S_ISDIR(info.st_mode))
			continue;

		if (!(buf = (char *)calloc(info.st_size, 1))) {
			say("unable to allocate memory for %s\n", chunk->d_name);
			continue;
		}

		int f = open(path, O_RDONLY);

		if (read(f, buf, info.st_size) != info.st_size) {
			say("warning: %s\n", strerror(errno));
		} else {
			// validate and cache the chunk
			compute_cid(cid, buf, info.st_size);

			if (strcmp(chunk->d_name, cid) != 0) {
				say("warning: %s is not a valid chunk\n", chunk->d_name);

			} else {
				say("caching %8lu bytes as %s\n", info.st_size, chunk->d_name);
				XputChunk(&xcache, buf, info.st_size, &addr);
			}
		}
		close(f);
		free(buf);
	}

	closedir(cd);
	XcacheHandleDestroy(&xcache);
}

int main(int argc, char **argv) {

	if (argc == 2 && strcmp(argv[1], "-q") == 0) {
		verbose = 0;
	} else if (argc != 1) {
		help(argv[0]);
	}

	say("Loading contents of %s\n", CACHEDIR);
	populate();

	return 0;
}
