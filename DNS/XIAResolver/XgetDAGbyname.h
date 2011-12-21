#define RR_XIA 65532
#define DNS_DEF_PORT 53
#define DNS_NOENTRY -1
#define DNS_FAILURE -2
#define ETC_HOSTS "/etc/hosts_xia"
#define RESOLV_CONF "/etc/resolv.xiaconf"

// DNS query header
typedef struct dns_message_header {
    short id;			// identification
    // flags 
    char rd :1;			// recursion desired
    char tc :1;			// trucation
    char aa :1;			// authoritative answer
    char opcode :4;		// purpose	
    char qr :1;			// question? response?
    char rcode :4;		// response code
    char z :3;			// reserve
    char ra :1;			// recursion available
    // counters
    short qdcount;		// # of questions
    short ancount;		// # of answers
    short nscount;		// # of authorities
    short arcount;		// # of additionals
} dns_mh_t;

// DNS query question
typedef struct dns_message_question {
    char *qname;		// domain name in dns format
    short qtype;		// type of query
    short qclass;		// class of query
} dns_mq_t;

// DNS resource record
typedef struct dns_rr {
    char *name;			// domain name in dns format
    short type;			// RR type code
    short rr_class;		// class
    int ttl;			// TTL
    short rdlength;		// data length
    char *rdata;		// record data
} dns_rr_t;

const char *XgetDAGbyname(char *name);
void nameconv_dnstonorm(char *dst, char *src);
int aaa();
