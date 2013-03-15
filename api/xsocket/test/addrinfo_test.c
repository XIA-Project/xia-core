#include <stdio.h>
#include <netdb.h>
#include "Xsocket.h"
#include "dagaddr.hpp"

#define SIZE	100
#define FULL_DAG	"RE %s %s %s"
#define HOST_DAG	"RE %s %s"
#define FULL_NAME	"fullname.test.xia"
#define HOST_NAME	"test.xia"
#define SID			"SID:0987654321098765432109876543210987654321"


int sock = -1;
int verbose = 1;
char hdag[1024];
char fdag[1024];

void die(const char *msg)
{
	if (sock >= 0)
		Xclose(sock);
	printf("%s", msg);
	exit(-1);
}

void setup()
{
	char ad[SIZE], hid[SIZE], fid[SIZE];
	sockaddr_x sa;

	sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if (sock < 0)
		die("Can't create a socket\n");

	if (XreadLocalHostAddr(sock, ad, SIZE, hid, SIZE, fid, SIZE) < 0)
		die("Can't get local address\n");

	sprintf(fdag, FULL_DAG, ad, hid, SID);
	Graph gf(fdag);
	gf.fill_sockaddr(&sa);
	if (XregisterName(FULL_NAME, &sa) != 0)
		die("Unable to register full test dag\n");

	sprintf(hdag, HOST_DAG, ad, hid);
	Graph gh(hdag);
	gh.fill_sockaddr(&sa);
	if (XregisterName(HOST_NAME, &sa) != 0)
		die("Unable to register host test dag\n");
}

void ai_print(const struct addrinfo *ai)
{
	if (!ai)
		return;

	printf("addrinfo:\n");
	printf("flags:     %d\n", ai->ai_flags);
	printf("family:    %d\n", ai->ai_family);
	printf("socktype:  %d\n", ai->ai_socktype);
	printf("protocol:  %d\n", ai->ai_protocol);
	printf("addrlen:   %d\n", ai->ai_addrlen);
	printf("addr:      %p\n", ai->ai_addr);
	printf("canonname: %s\n", ai->ai_canonname);
	printf("next:      %p\n", ai->ai_next);

	if (ai->ai_addr) {
		sockaddr_x *sx = (sockaddr_x *)ai->ai_addr;

		Graph g(sx);
		g.print_graph();
	}
}

void do_test(int expect, const char *id, const char *name, const char *service, const struct addrinfo *hints)
{
	struct addrinfo *ai;

	int rc = Xgetaddrinfo(name, service, hints, &ai);
	int success = (expect == 1 ? (rc == 0 ? 1 : 0) : (rc == 0 ? 0 : 1));

	printf("test: %s ", id);
	printf("name:%s ", name ? "yes" : "no");
	printf("service:%s ", service ? "yes" : "no");
	printf("hints:%s ", hints ? "yes" : "no");
	printf("expected:%s ", expect ? "pass" : "fail");
	printf(": %s\n", success ? "OK" : "FAIL");

	if (rc == 0) {
		if (verbose) {
			if (hints) {
				printf("HINTS: ");
				ai_print(hints);
				printf("\n");
			}
			printf("RESULT ");
			ai_print(ai);
			printf("\n");
		}
	} else {
		printf("failed: %s\n", Xgai_strerror(rc));
	}

	Xfreeaddrinfo(ai);

	if (!success) {
		// test failed
		printf("Unexpected result in tests, aborting...\n");
		exit(-1);
	}
}

int main()
{
	struct addrinfo hints;

	setup();


	// simple test, just DNS name lookup
	do_test(1, "1", FULL_NAME, NULL, NULL);

	// another simple DNS name lookup
	do_test(1, "2", HOST_NAME, NULL, NULL);

	// should fail, service lookup not implemented yet
	do_test(0, "3", HOST_NAME, SID, NULL);



	memset(&hints, 0, sizeof(hints));
	// should fail, similar to above, have hints, but flags not set
	do_test(0, "4", HOST_NAME, SID, &hints);

	// flags set, should add service to returned dag
	hints.ai_flags = XAI_XIDSERV;
	do_test(1, "5", HOST_NAME, SID, &hints);

	// should fail, expecing service to be set due to flags
	do_test(0, "6", HOST_NAME, NULL, &hints);



	memset(&hints, 0, sizeof(hints));
	// should all pass, using valid socktype
	hints.ai_socktype = XSOCK_STREAM;
	do_test(1, "7", HOST_NAME, NULL, &hints);
	hints.ai_socktype = XSOCK_CHUNK;
	do_test(1, "8", HOST_NAME, NULL, &hints);
	hints.ai_socktype = XSOCK_DGRAM;
	do_test(1, "9", HOST_NAME, NULL, &hints);

	// should fail, raw not supported
	hints.ai_socktype = XSOCK_RAW;
	do_test(0, "9", HOST_NAME, NULL, &hints);	



	memset(&hints, 0, sizeof(hints));
	// should fail, protocols not supported right now
	hints.ai_protocol = 1;
	do_test(0, "10", HOST_NAME, NULL, &hints);	



	memset(&hints, 0, sizeof(hints));
	// should fail, protocols not supported right now
	hints.ai_family = AF_INET;
	do_test(0, "11", HOST_NAME, NULL, &hints);	

	// should pass, family is OK
	hints.ai_family = AF_XIA;
	do_test(1, "12", HOST_NAME, NULL, &hints);

	// should pass, UNSPEC is OK
	hints.ai_family = AF_UNSPEC;
	do_test(1, "13", HOST_NAME, NULL, &hints);



	memset(&hints, 0, sizeof(hints));
	// should fail, 
	hints.ai_flags = AI_CANONNAME;
	do_test(0, "14", NULL, NULL, &hints);	


	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_DAGHOST;
	// passing in dag instead of name, fails because no name specified
	do_test(0, "15", NULL, NULL, &hints);	
	// passes, have name
	do_test(1, "16", hdag, NULL, &hints);

	// specifying host dag and sid should work
	hints.ai_flags |= XAI_XIDSERV;
	do_test(1, "17", hdag, SID, &hints);



	memset(&hints, 0, sizeof(hints));

	// should get our own address back
	do_test(1, "18", NULL, NULL, NULL);
	do_test(1, "19", NULL, NULL, &hints);
	hints.ai_flags |= AI_PASSIVE;
	do_test(1, "20", NULL, NULL, &hints);
	hints.ai_flags |= XAI_XIDSERV;
	do_test(1, "21", NULL, SID, &hints);

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags |= XAI_FALLBACK;
	// should get our own address back wrapped in parens
	do_test(1, "22", NULL, NULL, &hints);

	printf("\nAll tests successful!\n");
	return 0;
}
