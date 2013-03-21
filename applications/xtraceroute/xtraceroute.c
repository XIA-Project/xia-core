/*
 *
 printf("in loop\n");
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
 * Changed argument to inet_ntoa() to be struct in_addr instead of u_long
 * DFM BRL 1992
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 *
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
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
#include "dagaddr.hpp"

#include "xip.h"

#define	MAXWAIT		10	/* max time to wait for response, sec. */
#define	MAXPACKET	4096	/* max packet size */
#define VERBOSE		1	/* verbose flag */
#define QUIET		2	/* quiet flag */
#define FLOOD		4	/* floodping flag */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif

#define HID0 "HID:0000000000000000000000000000000000000000"
#define HID1 "HID:0000000000000000000000000000000000000001"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define RHID0 "HID:0000000000000000000000000000000000000002"
#define RHID1 "HID:0000000000000000000000000000000000000003"
#define CID0 "CID:2000000000000000000000000000000000000000"
#define CID1 "CID:2000000000000000000000000000000000000001"
#define SID0 "SID:0f00000000000000000000000000000000000055"

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
extern	int errno;

int s;			/* Socket file descriptor */
struct hostent *hp;	/* Pointer to host info */
struct timezone tz;	/* leftover */

sockaddr_x whereto;
size_t datalen;		/* How much data */

char usage[] =
"Usage: xtraceroute [-dfqrv] host [packetsize [count [preload]]]\n";

char *hostname;
char hnamebuf[MAXHOSTNAMELEN];

int npackets;
int preload = 0;		/* number of packets to "preload" */
int ntransmitted = 0;		/* sequence # for outbound packets = #sent */
int ident;

int nreceived = 0;		/* # of packets we got back */
int timing = 0;
int tmin = 999999999;
int tmax = 0;
int tsum = 0;			/* sum of all times, for doing average */
char *inet_ntoa();

/*
 * 			M A I N
 */
int main(int argc, char **argv)
{
    sockaddr_x from;
	char **av = argv;
	socklen_t len;
	char s_to[1024], s_from[1024];

	argc--, av++;
	while (argc > 0 && *av[0] == '-') {
		while (*++av[0]) switch (*av[0]) {
			case 'd':
				options |= SO_DEBUG;
				break;
			case 'r':
				options |= SO_DONTROUTE;
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
		}
		argc--, av++;
	}
	if(argc < 1 || argc > 4)  {
	  printf("%s",usage);
		exit(1);
	}

	len = sizeof(whereto);
	if (XgetDAGbyName(av[0], &whereto, &len) < 0) {
	  printf("Error Resolving XID\n");
	  exit(-1);
	}

	if( argc >= 2 )
		datalen = atoi( av[1] );
	else
		datalen = 64-8;
	if (datalen > MAXPACKET) {
		fprintf(stderr, "ping: packet size too large\n");
		exit(1);
	}
	if (datalen >= sizeof(struct timeval))	/* can we time 'em? */
		timing = 1;

	if (argc >= 3)
		npackets = atoi(av[2]);

	if (argc == 4)
		preload = atoi(av[3]);

	ident = getpid() & 0xFFFF;

	if((s = Xsocket(AF_XIA, SOCK_RAW, 0)) < 0) {
	  perror("ping: socket");
	  exit(5);
	}

	int nxt = XPROTO_XCMP;
	if (Xsetsockopt(s, XOPT_NEXT_PROTO, (const void*)&nxt, sizeof(nxt)) < 0) {
	  printf("Xsetsockopt failed on XOPT_NEXT_PROTO\n");
	  exit(-1);
	}

	Graph g(&whereto);
	strncpy(s_to, g.dag_string().c_str(), sizeof(s_to));
	printf("TRACEROUTE %s:\n", s_to);

	setlinebuf( stdout );

	signal( SIGINT, (sighandler_t)finish );
	signal(SIGALRM, (sighandler_t)catcher);

	/* fire off them quickies */
	for(i=0; i < preload; i++)
		pinger();

	if(!(pingflags & FLOOD))
		catcher();	/* start things going */

	for (;;) {
		int len = sizeof (packet);
		socklen_t fromlen = sizeof (from);
		int cc;
		struct timeval timeout;
		int fdmask = 1 << s;

		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;

		if(pingflags & FLOOD) {
			pinger();
			if( select(32, (fd_set *)&fdmask, 0, 0, &timeout) == 0)
				continue;
		}
		if ( (cc=Xrecvfrom(s, packet, len, 0, (struct sockaddr *)&from, &fromlen)) < 0) {
			if( errno == EINTR )
				continue;
			perror("ping: recvfrom");
			continue;
		}

		Graph gf(&from);
		strncpy(s_from, gf.dag_string().c_str(), sizeof(s_from));
		pr_pack( packet, cc, s_from);
		if(!strcmp(s_to,s_from)) finish();
		if (npackets && nreceived >= npackets)
			finish();
	}
	/*NOTREACHED*/
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
	if (npackets == 0 || ntransmitted < npackets)
		alarm(1);
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
	register struct timeval *tp = (struct timeval *) &outpack[8];
	register u_char *datap = &outpack[8+sizeof(struct timeval)];

	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = ntransmitted++;
	icp->icmp_id = ident;		/* ID */

	cc = datalen+8;			/* skips ICMP portion */

	if (timing)
		gettimeofday( tp, &tz );

	for( i=8; i<datalen; i++)	/* skip 8 for time */
		*datap++ = i;

	/* Compute ICMP checksum here */
	icp->icmp_cksum = in_cksum( icp, cc );

	
	// Set the TTL on the packet so we can use this like traceroute
	if(Xsetsockopt(s, XOPT_HLIM, (const void *)&ttl, sizeof(ttl)) < 0) {
	  printf("Xsetsockop failed on XOPT_HLIM\n");
	  exit(-1);
	}

	ttl++;


	rc = Xsendto(s, outpack, cc, 0, (struct sockaddr *)&whereto, sizeof(whereto));

	if( rc < 0 || rc != cc )  {
		if( rc<0 )  perror("sendto");
		printf("ping: wrote %s %d chars, ret=%d\n",
			hostname, cc, rc );
		fflush(stdout);
	}
	if(pingflags == FLOOD) {
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
pr_type( register int t )
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

	if( t < 0 || t > 16 )
		return("OUT-OF-RANGE");

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
	struct timeval *tp;
	int hlen, triptime;

	gettimeofday( &tv, &tz );

	xp = (struct xip *) buf;
	
	hlen = sizeof(struct xip) + sizeof(struct xia_xid_node) * (xp->dnode+xp->snode);
	
	if (cc < hlen + ICMP_MINLEN) {
		if (pingflags & VERBOSE)
			printf("packet too short (%d bytes) from %s\n", cc,
				   from); /* DFM */
		return;
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
	
	if( (!(pingflags & QUIET)) && icp->icmp_type != ICMP_ECHOREPLY )  {
		printf("%s\n",
		       from
		       );
		if (pingflags & VERBOSE) {
			for( i=0; i<12; i++)
			    printf("x%2.2x: x%8.8x\n", (unsigned int)(i*sizeof(long)),
					   (unsigned int)*lp++);
		}
		return;
	}
	if( icp->icmp_id != ident )
		return;			/* 'Twas not our ECHO */

	if (timing) {
		tp = (struct timeval *)&icp->icmp_data[0];
		tvsub( &tv, tp );
		triptime = tv.tv_sec*1000+(tv.tv_usec/1000);
		tsum += triptime;
		if( triptime < tmin )
			tmin = triptime;
		if( triptime > tmax )
			tmax = triptime;
	}

	if(!(pingflags & QUIET)) {
		if(pingflags != FLOOD) {
		  printf("%d bytes from %s: icmp_seq=%d", cc,
		  	       from,
		  	  icp->icmp_seq );	/* DFM */
		  	if (timing) 
				printf(" time=%d ms\n", triptime );
			else
				putchar('\n');
		} else {
			putchar('b');
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
	while( nleft > 1 )  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if( nleft == 1 ) {
		u_short	u = 0;

		*(u_char *)(&u) = *(u_char *)w ;
		sum += u;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
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
	if( (out->tv_usec -= in->tv_usec) < 0 )   {
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
	fflush(stdout);
	exit(0);
}
