#include "Xsocket.h"
#include <time.h>
//#include "Xinit.h"
//#include "Xutil.h"

typedef struct {
	int sock;
	int ttl_in;
	int ttl_out;
} testdata;

testdata data[10];

int main()
{
	unsigned i;

	socklen_t len;

	printf("[s|g]etsockopt test\n");

	srand(time(NULL));

	for (i = 0; i < sizeof(data) / sizeof(testdata); i++) {

		if ((data[i].sock = Xsocket(XSOCK_DGRAM)) <= 0) {
			printf("Error creating socket\n");
			exit(-1);
		}
		int ttl = data[i].ttl_in = rand() % 255;

		if (Xsetsockopt(data[i].sock, XOPT_HLIM, (const void *)&ttl, sizeof(ttl)) < 0) {
			printf("error setting ttl\n");
			exit(-1);
		}
	}

	for (i = 0; i < sizeof(data) / sizeof(testdata); i++) {
		len = sizeof(int);
		if (Xgetsockopt(data[i].sock, XOPT_HLIM, (void *)&data[i].ttl_out, &len) < 0) {
			printf("error getting ttl\n");
			exit(-1);
		}
	}

	for (i = 0; i < sizeof(data) / sizeof(testdata); i++) {
    	Xclose(data[i].sock);
//		printf("xsock:%3d set ttl:%3d get ttl:%3d\n", data[i].sock, data[i].ttl_in, data[i].ttl_out);
		if (data[i].ttl_in != data[i].ttl_out)
			printf("Error: ttl doesn't match!!!\n\n");
	}

	int sock = Xsocket(XSOCK_RAW);
	int nxt = XPROTO_XCMP;
	len = sizeof(int);

	if (sock < 0) {
		printf("error creating raw socket\n");
		exit(-1);
	}

	nxt = 0;
	if (Xgetsockopt(sock, XOPT_NEXT_PROTO, (void *)&nxt, &len) < 0) {
		printf("Xgetsockopt failed on XOPT_NEXT_PROTO\n");
		exit(-1);
	}
	if (nxt != XPROTO_XIA_TRANSPORT) {
		printf("Error:proto doesn't match!!!\n\n");
		exit(-1);
	}

	nxt = XPROTO_XCMP;

	if (Xsetsockopt(sock, XOPT_NEXT_PROTO, (const void*)&nxt, sizeof(nxt)) < 0) {
		printf("Xsetsockopt failed on XOPT_NEXT_PROTO\n");
		exit(-1);
	}

	nxt = 0;
	if (Xgetsockopt(sock, XOPT_NEXT_PROTO, (void *)&nxt, &len) < 0) {
		printf("Xgetsockopt failed on XOPT_NEXT_PROTO\n");
		exit(-1);
	}

	if (nxt != XPROTO_XCMP) {
		printf("Error:proto doesn't match!!!\n\n");
		exit(-1);
	}

	printf("Tests successful\n");
	Xclose(sock);

    return 0;
}

