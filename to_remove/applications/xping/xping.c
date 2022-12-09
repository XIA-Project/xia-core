/*
 *			X C M P P I N G . C
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
// Modified for XIA

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

#define VER_STR     "xping 1.1"

#define	MAXWAIT		10		// max time to wait for response, sec.
#define	MAXPACKET	1024	// max packet size
#define VERBOSE		0x01	// verbose flag
#define QUIET		0x02	// quiet flag
#define FLOOD		0x04	// floodping flag

typedef void (*sighandler_t)(int);

void pinger();
void catcher();
void finish();
void pr_pack(u_char *buf, int cc, char *from);
u_short in_cksum(struct icmp *addr, int len);
void tvsub(struct timeval *out, struct timeval *in);


u_char	packet[MAXPACKET];
int	i, pingflags, options;

int s;				// Socket file descriptor
struct hostent *hp;	// Pointer to host info
struct timezone tz;	// leftover

size_t datalen;		// How much data
sockaddr_x whereto;
sockaddr_x wherefrom;

char *hostname;
int npackets;
int preload = 0;		// number of packets to "preload"
int ntransmitted = 0;	// sequence # for outbound packets = #sent
unsigned short ident;

int nreceived = 0;		// # of packets we got back
int timing = 0;
unsigned tmin = 999999999;
unsigned tmax = 0;
unsigned tsum = 0;		// sum of all times, for doing average

float nCatcher = 0;
float catcher_timeout = 0; // seconds
float interval = 1;
int rc = 0;
int srcSet = 0;

void help()
{
	printf("xping [-fqrvV] [-c COUNT] [-i INTERVAL] [-l PRELOAD] [-s PKTSIZE] host\n\n");

	printf("options:\n");
	printf("  -c COUNT\n");
	printf("     stop after sending pings\n");
	printf("  -h, --help\n");
	printf("     display this help\n");
	printf("  -f flood ping. For every ECHO_REQUEST sent a period '.' is printed,\n");
	printf("     while for ever ECHO_REPLY received a backspace is printed. This\n");
	printf("     provides a rapid display of how many packets are being dropped.\n");
	printf("  -i INTERVAL\n");
	printf("     wait interval seconds between packets\n");
	printf("  -l PRELOAD\n");
	printf("     fire off PRELOAD packets immediately\n");
	printf("     wait interval seconds between pings\n");
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
	char buf[XIA_MAX_DAG_STR_SIZE];
	sockaddr_x from;
	char **av = argv;
	socklen_t len;

	datalen = 64 - 8;

	argc--, av++;
	while (argc > 0 && *av[0] == '-') {
		int c = argc;
		while (*++av[0]) {
			if (c != argc) {
				break;
			}

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
			case 'v':
				pingflags |= VERBOSE;
				break;
			case 'q':
				pingflags |= QUIET;
				break;
			case 'f':
				pingflags |= FLOOD;
				break;
			case 'i':
				argc--, av++;
				interval = atof(av[0]);
				break;
			case 'c':
				argc--, av++;
				npackets = atoi(av[0]);
				break;
			case 'l':
				argc--, av++;
				preload = atoi(av[0]);
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
			// 	len = sizeof(wherefrom);
			// 	srcSet = 1;
			// 	if (XgetDAGbyName(av[0], &wherefrom, &len) < 0) {
			// 		printf("Error Resolving XID\n");
			// 		exit(-1);
			// 	}
			// 	break;
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
	if (argc != 1)  {
		help();
	}

	len = sizeof(whereto);
	if (xia_pton(AF_XIA, av[0], &whereto) == 1) {
		// we've got it
	} else if (XgetDAGbyName(av[0], &whereto, &len) < 0) {
	  printf("Error Resolving XID\n");
	  exit(-1);
	}

	if (datalen > MAXPACKET) {
		fprintf(stderr, "ping: packet size too large: max allowed size = %d\n", MAXPACKET);
		exit(1);
	}
	if (datalen >= 8) {
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
			printf("Xbind failed\n");
			exit(-1);
		}
	}

	xia_ntop(AF_XIA, &whereto, buf, sizeof(buf));
	printf("PING %s: %ld data bytes\n\n", buf, (long int)datalen);

	setlinebuf(stdout);

	signal(SIGINT, (sighandler_t)finish);
	signal(SIGALRM, (sighandler_t)catcher);

	// fire off them quickies
	for (i=0; i < preload; i++) {
		pinger();
	}

	if (!(pingflags & FLOOD)) {
		catcher();	// start things going
	}

	for (;;) {
		int len = sizeof (packet);
		socklen_t fromlen = sizeof (from);
		int cc;
		struct timeval timeout;
		fd_set fdmask;

		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;

		if (pingflags & FLOOD) {
			pinger();
			FD_SET(s, &fdmask);
			if (Xselect(s + 1, &fdmask, NULL, NULL, &timeout) == 0)  {
				continue;
			}
		}
		if ((cc = Xrecvfrom(s, packet, len, 0, (struct sockaddr*)&from, &fromlen)) < 0) {
			if (errno == EINTR) {
				continue;
			}
			perror("ping: recvfrom");
			continue;
		}
		inet_ntop(AF_XIA, &from, buf, sizeof(buf));
		pr_pack(packet, cc, buf);
		if (npackets && nreceived >= npackets) {
			finish();
		}
	}
	/*NOTREACHED*/
}

/*
 * 			C A T C H E R
 *
 * This routine causes another PING to be transmitted, and then
 * schedules another SIGALRM for interval seconds from now.
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
	nCatcher += interval;
	if (catcher_timeout && nCatcher >= catcher_timeout) {
		rc = nreceived ? 0 : -1;
		finish();
	}
	if (npackets == 0 || ntransmitted < npackets) {
		if (interval < 1) {
			ualarm((int)(interval*1000000), 0);
		} else {
			alarm((int)interval);
		}
	} else {
		if (nreceived) {
			waittime = 2 * tmax / 1000;
			if (waittime == 0) {
				waittime = 1; //interval;
			}
		} else {
			waittime = MAXWAIT;
		}
		signal(SIGALRM, (sighandler_t)finish);
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
 */
void pinger()
{
	static u_char outpack[MAXPACKET];
	register struct icmp *icp = (struct icmp *)outpack;
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
		*(unsigned *)p  = htonl(tp.tv_sec);
		p = &outpack[12];
		*(unsigned *)p = htonl(tp.tv_usec);
	}

	// skip 8 for time
	for (i = 8; i < datalen; i++) {
		*datap++ = i;
	}

	// Compute ICMP checksum here
	icp->icmp_cksum = in_cksum(icp, cc);

	rc = Xsendto(s, outpack, cc, 0, (struct sockaddr*)&whereto, sizeof(sockaddr_x));

	if (rc < 0 || rc != cc) {
		if (rc < 0) {
			perror("sendto");
		}
		printf("ping: wrote %s %d chars, ret=%d\n", hostname, cc, rc);
		fflush(stdout);
	}
	if (pingflags == FLOOD) {
		putchar('.');
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
void pr_pack(u_char *buf, int cc, char *from)
{
	struct xip *xp;
	register struct icmp *icp;
	register long *lp = (long *) packet;
	register int i;
	struct timeval tv;
	struct timeval tp;
	int hlen;
	unsigned triptime = 0;

	gettimeofday(&tv, &tz);

	xp = (struct xip *)buf;

	hlen = sizeof(struct xip) + sizeof(struct xia_xid_node) * (xp->dnode+xp->snode);

	if (cc < hlen + ICMP_MINLEN) {
		if (pingflags & VERBOSE) {
			printf("packet too short (%d bytes) from %s\n", cc, from);
		}
		return;
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);


	if ((!(pingflags & QUIET)) && icp->icmp_type != ICMP_ECHOREPLY)  {
		printf("%d bytes from %s: icmp_type=%d (%s) icmp_code=%d\n", cc, from,
		  icp->icmp_type, pr_type(icp->icmp_type), icp->icmp_code);

		if (pingflags & VERBOSE) {
			for(i = 0; i < 12; i++) {
				printf("x%2.2x: x%8.8x\n", (unsigned int)(i*sizeof(long)), (unsigned int)*lp++);
		   }
		}
		return;
	}

	// both are in network byte order
	if (icp->icmp_id != ident) {
		return;			// 'Twas not our ECHO
	}

	if (timing) {
		int *t = (int *)&icp->icmp_data[0];

		tp.tv_sec  = ntohl(*t);
		t = (int *)&icp->icmp_data[4];
		tp.tv_usec  = ntohl(*t);

		tvsub(&tv, &tp);
		triptime = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
		tsum += triptime;
		if (triptime < tmin) {
			tmin = triptime;
		}
		if (triptime > tmax) {
			tmax = triptime;
		}
	}

	if (!(pingflags & QUIET)) {
		if (pingflags != FLOOD) {
			printf("bytes=%d icmp_seq=%d", cc, ntohs(icp->icmp_seq));
			if (timing) {
				printf(" time=%d ms\n", triptime);
			} else {
				putchar('\n');
			}
			printf("from %s\n\n", from);

		} else {
			putchar('\b');
			fflush(stdout);
		}
	}
	nreceived++;
}


/*
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
u_short in_cksum(struct icmp *addr, int len)
{
	register int nleft = len;
	register u_short *w = (u_short *)addr;
	register u_short answer;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
				  */
	while(nleft > 1)  {
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
	answer = ~sum;				// truncate to 16 bits
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
 * Print out statistics, and give up.
 * Heavily buffered STDIO is used here, so that all the statistics
 * will be written with 1 sys-write call.  This is nice when more
 * than one copy of the program is running on a terminal;  it prevents
 * the statistics output from becomming intermingled.
 */
void finish()
{
	putchar('\n');
	fflush(stdout);
	printf("\n----%s PING Statistics----\n", hostname);
	printf("%d packets transmitted, ", ntransmitted);
	printf("%d packets received, ", nreceived);
	if (ntransmitted) {
		if (nreceived > ntransmitted) {
			printf("-- somebody's printing up packets!");
		} else {
			printf("%d%% packet loss", (int) (((ntransmitted-nreceived) * 100) / ntransmitted));
		}
	}
	printf("\n");
	if (nreceived && timing) {
		printf("round-trip (ms)  min/avg/max = %d/%d/%d\n", tmin, tsum / nreceived, tmax);
	}
	fflush(stdout);

	Xclose(s);
	exit(rc);
}
