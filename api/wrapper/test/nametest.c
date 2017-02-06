#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>


// struct hostent {
//     char  *h_name;            /* official name of host */
//     char **h_aliases;         /* alias list */
//     int    h_addrtype;        /* host address type */
//     int    h_length;          /* length of address */
//     char **h_addr_list;       /* list of addresses */
// }
// #define h_addr h_addr_list[0] /* for backward compatibility */


const char *names[] = {
	"badname.foo",
	"cmu.edu",
	"google.com",
	"r0",
	NULL
};

const char *addrs[] = {
	"192.168.111.111",
	"10.2.0.131",
	"128.2.42.10",
	"72.38.100.1",
	NULL
};

void dump(struct hostent *he)
{
	char **p;

	if (!he)
		return;

	if (he->h_name)
		printf("      name:%s\n", he->h_name);

	if (he->h_aliases) {;
		p = he->h_aliases;

		while (*p) {
			printf("      alias:%s\n", *p);
			p++;
		}
	}

	printf("      addrtype:%d\n", he->h_addrtype);
	printf("      length: %d\n", he->h_length);

	if (he->h_addr_list) {
		p = he->h_addr_list;
		while (*p) {
			char buf[256];
			struct in_addr *ia = ((struct in_addr *)*p);
			 inet_ntop(AF_INET, ia, buf, sizeof(buf));
			printf("      addr:%s\n\n", buf);
			p++;
		}
	}
}


void status(const char *name, struct hostent *he, int *rc, int *ec)
{
	if (he == NULL) {
		printf("   %-20s: error (%s)", name, hstrerror(h_errno));
		if (rc) {
			printf(" rc:%d", *rc);
		}
		if (ec) {
			printf(" ec:%d", *ec);
		}

	} else {
		printf("   %-20s: success", name);
	}

	printf("\n");
	dump(he);
}


void gethostbyname_test(const char *name)
{
	struct hostent *he;

	he = gethostbyname(name);
	status(name, he, NULL, NULL);
}

void gethostbyname_r_test(const char *name)
{
	static char buf[1024];
	static struct hostent he;
	struct hostent *result;
	int err;

	int rc = gethostbyname_r(name, &he, buf, sizeof(buf), &result, &err);
	status(name, result, &rc, &err);
}

void gethostbyname2_r_test(const char *name)
{
	static char buf[1024];
	static struct hostent he;
	struct hostent *result;
	int err;

	int rc = gethostbyname2_r(name, AF_INET, &he, buf, sizeof(buf), &result, &err);
	status(name, result, &rc, &err);
}


void gethostbyname2_test(const char *name)
{
	struct hostent *he;

	he = gethostbyname2(name, AF_INET);
	status(name, he, NULL, NULL);
}

void gethostbyaddr_test(const char *name)
{
	struct hostent *he;
	struct in_addr addr;

	inet_pton(AF_INET, name, &addr);
	he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
	status(name, he, NULL, NULL);
}

void gethostbyaddr_r_test(const char *name)
{
	static char buf[1024];
	static struct hostent he;
	struct hostent *result;
	struct in_addr addr;
	int err;

	inet_pton(AF_INET, name, &addr);
	int rc = gethostbyaddr_r(&addr, sizeof(addr), AF_INET, &he, buf, sizeof(buf), &result, &err);
	status(name, result, &rc, &err);
}

int main()
{
	printf("\ngethostbyname:\n");
	for (const char **name = names; *name; name++) {
		gethostbyname_test(*name);
	}
	for (const char **addr = addrs; *addr; addr++) {
		gethostbyname_test(*addr);
	}

	printf("\ngethostbyname_r:\n");

	for (const char **name = names; *name; name++) {
		gethostbyname_r_test(*name);
	}
	for (const char **addr = addrs; *addr; addr++) {
		gethostbyname_r_test(*addr);
	}

	// printf("gethostbyname2:\n");
	// for (const char **name = names; *name; name++) {
	// 	gethostbyname2_test(*name);
	// }
	// for (const char **addr = addrs; *addr; addr++) {
	// 	gethostbyname2_test(*addr);
	// }

	// printf("gethostbyname2_r:\n");
	// for (const char **name = names; *name; name++) {
	// 	gethostbyname2_r_test(*name);
	// }
	// for (const char **addr = addrs; *addr; addr++) {
	// 	gethostbyname2_r_test(*addr);
	// }

	printf("\ngethostbyaddr:\n");
	for (const char **addr = addrs; *addr; addr++) {
		gethostbyaddr_test(*addr);
	}

	printf("\ngethostbyaddr_r:\n");
	for (const char **addr = addrs; *addr; addr++) {
		gethostbyaddr_r_test(*addr);
	}

	return 0;
}
