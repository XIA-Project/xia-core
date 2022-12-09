#define RR_TYPE_XIA 65532		// arbitrary rr type code, subject to change

typedef struct dns_header {
	short id;			// identification

	// flags 
	char rd :1;		// recursion desired
	char tc :1;		// trucation
	char aa :1;		// authoritative answer
	char opcode :4;	// purpose	
	char qr :1;		// question? response?

	char rcode :4;		// response code
	char z :3;		// reserve
	char ra :1;		// recursion available
	
	// counters
	short qdcount;		// # of questions
	short ancount;		// # of answers
	short nscount;		// # of authorities
	short arcount;		// # of additionals
} dns_header_t;

typedef struct dns_question {
	char *qname;		// domain name
	short qtype;		// type of query
	short qclass;		// class of query
} dns_question_t;

typedef struct dns_rr {
	char *name;		// domain name
	short type;		// RR type code
	short rr_class;	// class of data
	int ttl;		// time to live
	short rdlength;	// data length
	char *rdata;		// record data
} dns_rr_t;

int getxiadstdagbyname(char *result, char *host, char *dns_server);
void hostname_conversion(char *dns_format, char *hostname);
