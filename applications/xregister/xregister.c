/* ts=4 */
/*
** Copyright 2015 Carnegie Mellon University
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
#include "Xsocket.h"
#include "dagaddr.hpp"

char port[8];
char address[16];
char hostname[64];
char dag[1024];
char id[72];

void help()
{
	printf("usage: xregister [-a address] [-n hostname] [-p port] dag\n");
	printf("where:\n");
	printf(" -a IP address (port must also be given)\n");
	printf(" -h hostname (port is optional)\n");
	printf(" -p port number\n");
	printf("\nUsed to create an ID to DAG mapping in the nameserver for applications");
	printf("\n  run inside of the xwrap application\n");
	printf("\nexample:\n%s\n\n", "xregister -a 10.0.0.1 -p 1115 RE AD:6b790c814eac5bb8318dbc2aaff7c5af77c3db6a HID:a7d1c7f3a64a12ce0b18515178c911968916f3c3 SID:1110000000000000000000000000000000001113");
	exit(-1);
}

void die(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "\nxregister -h for help\n\n");
	exit(-1);
}

void configure(int argc, char** argv)
{
	int c;
	opterr = 0;
	bool haveport, haveaddr, havehost;
	haveport = haveaddr = havehost = false;

	while ((c = getopt(argc, argv, "ha:n:p:")) != -1) {
		switch (c) {
			case 'a':
				strncpy(address, optarg, sizeof(address));
				haveaddr = true;
				break;
			case 'p':
				strncpy(port, optarg, sizeof(port));
				haveport = true;
				break;
			case 'n':
				strncpy(hostname, optarg, sizeof(hostname));
				havehost = true;
				break;
			case 'h':
			default:
				help();
		}
	}

	if (!haveaddr && !havehost) {
		die("you must specify a hostname or IP address\n");

	} if (haveaddr && havehost) {
		die("you must specify either a hostname or IP address, not both\n");

	} else if (haveaddr && !haveport) {
		die("you must specify a port number as awell as an IP address\n");

	} else if (haveport && !haveaddr && !havehost) {
		die("you must specify an IP address or hostname in addition to a port number\n");

	} else if (optind == argc) {
		// error no dag specified
		die("no dag specified!\n");

	}

	dag[0] = 0;
	int len = sizeof(dag) - 1;	// account for null terminator

	for (int i = optind; i < argc; i++) {
		if (i != optind) {
			strncat(dag, " ", len);
			len--;
		}
		strncat(dag, argv[i], len);
		len -= strlen(argv[i]);
		if (len <= 0)
			break;
	};

	if (haveport) {
		sprintf(id, "%s-%s", (haveaddr ? address : hostname), port);
	} else {
		strcpy(id, hostname);
	}
}

int main(int argc, char **argv)
{
	sockaddr_x sa;

	configure(argc, argv);

	Graph g(dag);;
	if (g.num_nodes() == 0) {
		die("invalid dag specified\n");
	}

	g.fill_sockaddr(&sa);

	if ( XregisterName(id, &sa) < 0) {
		die("Unable to register id %s\n", id);
	} else {
		printf("registered id:%s as\n%s\n", id, dag);
	}

	return 0;
}
