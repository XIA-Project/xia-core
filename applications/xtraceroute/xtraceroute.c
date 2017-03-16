/*
 *			X C M P T R A C E R O U T E . C
 *
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 *
 * Bugs -
 *	More statistics could always be gathered.
 */
// modified from PING source code to act as a rudimentery traceroute for XIA

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "Xsocket.h"

#include "xip.h"

#define VER_STR     "xtraceroute 1.1"

#define	MAXWAIT		10		// max time to wait for response, sec.
#define	MAXPACKET	4096	// max packet size
#define VERBOSE		0x1		// verbose flag
#define QUIET		0x2		// quiet flag
#define FLOOD		0x4		// floodping flag

typedef void (*sighandler_t)(int);

void pinger();
void catcher();
void finish();
void pr_pack(u_char *buf, int cc, const char *from);
void tvsub(struct timeval *out, struct timeval *in);
int in_cksum(struct icmp *addr, int len);

int ttl = 2;

u_char	packet[MAXPACKET];
int	i, pingflags, options;

int s;				// Socket file descriptor
struct hostent *hp;	// Pointer to host info
struct timezone tz;	// leftover

size_t datalen;		// How much data
sockaddr_x whereto;
sockaddr_x wherefrom;
socklen_t fromlen;

char usage[] = "Usage: xtraceroute [-fqrv] host [packetsize [max hops]]\n";

char *hostname;
int npackets = 30;		// max number of hops allowed
int preload = 0;		// number of packets to "preload"
int ntransmitted = 0;	// sequence # for outbound packets = #sent
unsigned short ident;

int nreceived = 0;		// # of packets we got back
int timing = 0;
int tmin = 999999999;
int tmax = 0;
int tsum = 0;			// sum of all times, for doing average

float nCatcher = 0;
float catcher_timeout = 0; // seconds
float interval = 0.5;
int rc = 0;
int srcSet = 0;


void help()
{
	printf("xtraceroute [-hqvV] [-m MAX HOPS] [-s PKTSIZE] host\n\n");

	printf("options:\n");
	printf("  -h, --help\n");
	printf("     display this help\n");
	printf("  -m MAX HOPS\n");
	printf("     Specifies the maximum number of hops to probe. (default = 30)\n");
	printf("  -q quiet mode\n");
	printf("  -s PKTSIZE\n");
	printf("     send packets of size PKTSIZE bytes\n");
	printf("  -v verbose output\n");
	printf("  -V  --version\n");
	printf("     display the version number\n");
	exit(0);
}


int main(int argc, char **argv)
{
	sockaddr_x from;
	char **av = argv;
	socklen_t len;
	char s_to[XIA_MAX_DAG_STR_SIZE], s_from[XIA_MAX_DAG_STR_SIZE];

	datalen = 64 - 8;

	argc--, av++;
	while (argc > 0 && *av[0] == '-') {
		int c = argc;
		while (*++av[0]) {
			if (c != argc)
				break;
			switch (*av[0]) {
			case '-':
				// see if it's a long option
				if (strcmp(av[0], "-version") == 0) {
					printf("%s\n", VER_STR);
					return 0;
				} else if  (strcmp(av[0], "-help") == 0) {
					help();
				}
				break;
			case 'm':
				argc--, av++;
				npackets = atoi(av[0]);
				break;
			case 'v':
				pingflags |= VERBOSE;
				break;
			case 'q':
				pingflags |= QUIET;
				break;
			case 's':
				argc--, av++;
				datalen = atoi(av[0]);
				break;
			// case 't':
			// 	argc--, av++;
			// 	catcher_timeout = atoi(av[0]);
			// 	break;
			// case 's':
			// 	argc--, av++;
			// 	fromlen = sizeof(wherefrom);
			// 	srcSet = 1;
			// 	if (XgetDAGbyName(av[0], &wherefrom, &fromlen) < 0) {
			// 		printf("Error Resolving XID\n");
			// 		exit(-1);
			// 	}
				break;
			case 'V':
				printf("%s\n", VER_STR);
				return 0;
			case 'h':
				help();
				break;
			}
		}
		argc--, av++;
	}
	if (argc != 1) {
		help();
		exit(1);
	}

	len = sizeof(whereto);
	if (xia_pton(AF_XIA, av[0], &whereto) == 1) {
		// we've got it
	} else if (XgetDAGbyName(av[0], &whereto, &len) < 0) {
	  printf("Error Resolving XID\n");
	  exit(-1);
	}

	if (datalen > MAXPACKET) {
		fprintf(stderr, "traceroute: packet size too large\n");
		exit(1);
	}
	if (datalen >= sizeof(struct timeval)) {
		// can we time 'em?
		timing = 1;
	}

	ident = htons(getpid() & 0xFFFF);

	if ((s = Xsocket(AF_XIA, SOCK_RAW, 0)) < 0) {
	  perror("ping: socket");
	  exit(5);
	}

	int nxt = XPROTO_XCMP;
	if (Xsetsockopt(s, XOPT_NEXT_PROTO, (const void*)&nxt, sizeof(nxt)) < 0) {
	  printf("Xsetsockopt failed on XOPT_NEXT_PROTO\n");
	  exit(-1);
	}

	if (srcSet) {
		if (Xbind(s, (struct sockaddr *)&wherefrom, sizeof(sockaddr_x)) < 0) {
			printf("Xbind failed");
			exit(-1);
		}
	}

	xia_ntop(AF_XIA, &whereto, s_to, sizeof(s_to));
	printf("TRACEROUTE (%u hops %lu bytes) to\n%s:\n\n", npackets, datalen, s_to);

	setlinebuf(stdout);

	signal(SIGINT, (sighandler_t)finish);
	signal(SIGALRM, (sighandler_t)catcher);

	catcher();	// start things going

	for (;;) {
		int len = sizeof (packet);
		socklen_t fromlen = sizeof (from);
		int cc;

		if ((cc=Xrecvfrom(s, packet, len, 0, (struct sockaddr *)&from, &fromlen)) < 0) {
			if (errno == EINTR) {
				continue;
			}
			perror("ping: recvfrom");
			continue;
		}

		xia_ntop(AF_XIA, &from, s_from, sizeof(s_from));
		pr_pack(packet, cc, s_from);


		if (!strcmp(s_to,s_from)){
			finish();
		}

		// FIXME: HACK!!!
		// if the 2 dags are equivelent, but the above comparison fails because the nodes of the
		// 2 dags are in different orders, isolate just the HIDs and compare them instead
		// This is not great, because in theory, xtraceroute should work with any final
		// intent including SID, AD, CID, etc.
		// THis hack only addresses the case where the intent is an HID _AND_ the HID is the last
		// node in the DAG.
		char *f = strstr(s_from, "HID:");
		char *t = strstr(s_to, "HID:");
		if (!strcmp(f, t)){
			finish();
		}
		// END HACK

		if (npackets && nreceived >= npackets) {
			finish();
		}
	}
	// NOTREACHED
}

/*
 * 			C A T C H E R
 *
 * This routine causes another PING to be transmitted, and then
 * schedules another SIGALRM for 1 second from now.
 *
 * Bug -
 * 	Our sense of time will slowly skew (ie, packets will not be launched
 * 	exactly at 1-second intervals).  This does not affect the quality
 *	of the delay and loss statistics.
 */
void catcher()
{
	int waittime;

	pinger();
	nCatcher+=interval;
	if (catcher_timeout && nCatcher >= catcher_timeout) {
		rc = -1;
		finish();
	}
	if (npackets == 0 || ntransmitted < npackets) {
		if (interval < 1) {
			ualarm((int)(interval*1000000),0);
		}
		else {
			alarm((int)interval);
		}
	}
	else {
		if (nreceived) {
			waittime = 2 * tmax / 1000;
			if (waittime == 0)
				waittime = 1;
		} else
			waittime = MAXWAIT;
		signal(SIGALRM,(sighandler_t) finish);
		alarm(waittime);
	}
}

/*
 * 			P I N G E R
 *
 * Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 *
 * we also keep incrmenting the packet TTL in order to emulate traceroute
 */
void pinger()
{
	static u_char outpack[MAXPACKET];
	register struct icmp *icp = (struct icmp *) outpack;
	unsigned i;
	int rc, cc;
	struct timeval tp;
	register u_char *datap = &outpack[8 + 8];

	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = htons(ntransmitted++);
	icp->icmp_id = ident;	// ID

	cc = datalen + 8;		// skips ICMP portion

	if (timing) {
		gettimeofday(&tp, &tz);
		void *p = &outpack[8];
		*(unsigned *)p = htonl(tp.tv_sec);
		p = &outpack[12];
		*(unsigned *)p = htonl(tp.tv_usec);
	}

	// skip 8 for time
	for(i = 8; i < datalen; i++) {
		*datap++ = i;
	}

	// Compute ICMP checksum here
	icp->icmp_cksum = in_cksum(icp, cc);


	// Set the TTL on the packet so we can use this like traceroute
	if (Xsetsockopt(s, XOPT_HLIM, (const void *)&ttl, sizeof(ttl)) < 0) {
	  printf("Xsetsockop failed on XOPT_HLIM\n");
	  exit(-1);
	}

	ttl++;

	rc = Xsendto(s, outpack, cc, 0, (struct sockaddr *)&whereto, sizeof(whereto));

	if (rc < 0 || rc != cc)  {
		if (rc < 0) {
			perror("sendto");
		}
		printf("ping: wrote %s %d chars, ret=%d\n", hostname, cc, rc);
		fflush(stdout);
	}
}

/*
 * 			P R _ T Y P E
 *
 * Convert an ICMP "type" field to a printable string.
 */
const char *
pr_type(register int t)
{
	static const char *ttab[] = {
		"Echo Reply",
		"ICMP 1",
		"ICMP 2",
		"Dest Unreachable",
		"Source Quench",
		"Redirect",
		"ICMP 6",
		"ICMP 7",
		"Echo",
		"ICMP 9",
		"ICMP 10",
		"Time Exceeded",
		"Parameter Problem",
		"Timestamp",
		"Timestamp Reply",
		"Info Request",
		"Info Reply"
	};

	if (t < 0 || t > 16) {
		return("OUT-OF-RANGE");
	}

	return(ttab[t]);
}

/*
 *			P R _ P A C K
 *
 * Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void pr_pack(u_char *buf, int cc, const char *from)
{
	struct xip *xp;
	register struct icmp *icp;
	register long *lp = (long *) packet;
	register int i;
	struct timeval tv;
	const char *s;
	// struct timeval tp;
	int hlen;
	// int triptime = 0;

	gettimeofday(&tv, &tz);

	xp = (struct xip *) buf;

	hlen = sizeof(struct xip) + sizeof(struct xia_xid_node) * (xp->dnode+xp->snode);

	if (cc < hlen + ICMP_MINLEN) {
		if (pingflags & VERBOSE) {
			printf("packet too short (%d bytes) from %s\n", cc, from);
		}
		return;
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);

	printf("hop %d", nreceived);

	if (icp->icmp_type == ICMP_TIMXCEED) {
		printf("\n%s\n", from);
	} else if (icp->icmp_type == ICMP_UNREACH) {
		switch(icp->icmp_code) {
			case 0:
				s = "network";
				break;
			case 1:
				s = "host";
				break;
			case 3:
				s = "intent";
				break;
			default:
				s = "???";
		}
		printf(" %s unreachable\n", s);
	} else {
		printf(" XCMP type:%u code:%u\n\n", icp->icmp_type, icp->icmp_code);
	}
	if (pingflags & VERBOSE) {
		for(i = 0; i < 12; i++) {
			printf("x%2.2x: x%8.8x\n", (unsigned int)(i * sizeof(long)), (unsigned int)*lp++);
		}
	}
	printf("\n");

	// if (timing) {
	// 	int *t = (int *)&icp->icmp_data[0];
	//
	// 	tp.tv_sec  = ntohl(*t);
	// 	t = (int *)&icp->icmp_data[4];
	// 	tp.tv_usec  = ntohl(*t);
	//
	// 	tvsub(&tv, &tp);
	// 	triptime = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
	// 	tsum += triptime;
	// 	if (triptime < tmin) {
	// 		tmin = triptime;
	// 	}
	// 	if (triptime > tmax) {
	// 		tmax = triptime;
	// 	}
	// }
	//
	//	if (timing) {
	//		printf(" time=%d ms\n", triptime);
	// } else {
	//		putchar('\n');
	// }
	nreceived++;
}


/*
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
int in_cksum(struct icmp *addr, int len)
{
	register int nleft = len;
	register u_short *w = (ushort *)addr;
	register u_short answer;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
				  */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	// mop up an odd byte, if necessary
	if (nleft == 1) {
		u_short	u = 0;

		*(u_char *)(&u) = *(u_char *)w ;
		sum += u;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	// add hi 16 to low 16
	sum += (sum >> 16);			// add carry
	answer = ~sum;				/* truncate to 16 bits */
	return answer;
}

/*
 * 			T V S U B
 *
 * Subtract 2 timeval structs:  out = out - in.
 *
 * Out is assumed to be >= in.
 */
void tvsub(struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
		out->tv_sec--;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 *			F I N I S H
 *
 * Just clean up
 */
void finish()
{
	Xclose(s);
	int hops = rc ? -1 : nreceived;
	printf("hops = %d\n", hops);
	fflush(stdout);
	exit(rc);
}
