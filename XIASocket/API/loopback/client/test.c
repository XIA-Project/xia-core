/*
* Tests various Xfunctions
* Doesn't work with actual DAGs
*/

#include "Xsocket.h"

int main(int argc, char *argv[])
{
	int sock;
	char buf[MAXBUFLEN];
	
	if (argc < 2) {
		fprintf(stderr, "ERROR, no port provided\n");
		exit(0);
	}
	
	sock=Xsocket("Source_DAG");
	if (sock==-1)
	{
		printf("fail\n");
	}
	else
	{
		printf("Xsocket: Success (sockfd=%d)\n",sock);
		char *sndbuf="sendto working";
		int nb= Xsendto(sock,sndbuf, sizeof(sndbuf), 0,0,0)	;
		printf("Xsendto: Success (sent:%d)\n",nb);
		nb= Xrecvfrom(sock,buf, sizeof(buf), 0,0,0)	;
		printf("%s",buf);
		Xclose(sock);

	}

	return 0;
}

