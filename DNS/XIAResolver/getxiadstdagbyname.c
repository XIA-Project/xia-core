#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "getxiadstdagbyname.h"

/*
 * get xia destination dag
 * returns
 *  0 on successful dag retreival
 * -1 on no entry found
 * -2 on connection error to dns
 */
int getxiadstdagbyname(char *result, char *host, char *dns_server) {
	int dnsfd;
	int res_addr_size;
	socklen_t i;
	struct sockaddr_in dns_addr;
	char request_buf[512];
	size_t req_buf_offset;
	char response_buf[512];
	size_t res_buf_offset;
	dns_header_t query_header;
	dns_question_t query_question;
	dns_header_t *response_header;

	// init dns server connection
	dnsfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	dns_addr.sin_family = AF_INET;
	dns_addr.sin_port = htons(53);		// standard dns port
	dns_addr.sin_addr.s_addr = inet_addr((char *)dns_server);

	// init dns message header
	query_header.qr = 0;			// question
	query_header.opcode = 0;		// standard query
	query_header.aa = 0;			// not authoritative
	query_header.tc = 0;			// no truncation
	query_header.rd = 1;			// recursion desired
	query_header.ra = 0;
	query_header.z = 0;
	query_header.rcode = 0;
	query_header.qdcount = htons(1);	// one question
	query_header.ancount = 0;
	query_header.nscount = 0;
	query_header.arcount = 0;

	// init dns message question
	query_question.qname = malloc(strlen(host)+2);		// buffer space for dns address format
	memset(query_question.qname, 0, strlen(host)+2);
	hostname_conversion(query_question.qname, host);
	query_question.qtype = htons(RR_TYPE_XIA);	// set to XIA DAG type
	query_question.qclass = htons(1);

	// send buffer creation
	memset(request_buf, 0, 512);
	memcpy(request_buf, &query_header, sizeof(query_header));
	req_buf_offset = sizeof(dns_header_t);
	memcpy(request_buf+req_buf_offset, query_question.qname, strlen(query_question.qname));
	req_buf_offset += strlen(query_question.qname) + 1;
	memcpy(request_buf+req_buf_offset, &query_question.qtype, sizeof(short));
	req_buf_offset += sizeof(short);
	memcpy(request_buf+req_buf_offset, &query_question.qclass, sizeof(short));
	req_buf_offset += sizeof(short);

	// receive dag information
	if (sendto(dnsfd, request_buf, req_buf_offset, 0, (struct sockaddr *) &dns_addr, sizeof(dns_addr)) == -1)
	{
		return -2;
	}
	if (recvfrom(dnsfd, response_buf, 512, 0, (struct sockaddr *) &dns_addr, &i) == -1)
	{
		return -2;
	}
	response_header = (dns_header_t *)response_buf;
	// if there is at least one answer
	if (ntohs(response_header->ancount) > 0)
	{
			res_buf_offset = sizeof(dns_header_t);
			// offset accounted for dns format host address(name before resolution
			res_buf_offset += strlen(query_question.qname)+1;
			// offset accounted for question type + class
			res_buf_offset += 2*sizeof(short);
			// offset according
			// case 1: pointer to previous name
			// case 2: raw name
			if ((ntohs(*(response_buf+res_buf_offset)) & 3<<14) == 3<<14) {
				res_buf_offset += sizeof(short);
			} else {
				res_buf_offset += strlen(response_buf+res_buf_offset)+1;
			}
			// offset accounted for type, class, TTL (2*4 bytes)
			res_buf_offset += 4*sizeof(short);
			// size of the answer
			res_addr_size = ntohs(*((unsigned short *)(response_buf+res_buf_offset)));
			memcpy(result, response_buf+res_buf_offset+2, res_addr_size);
			result[res_addr_size] = '\0';
			free(query_question.qname);
	}
	else
	{
		result[0] = '\0';
		return -1;
	}
	return 0;
}

// convert host name to dns format
void hostname_conversion(char *dns_format, char *hostname) {
	int i, j;
	j=0;
	for (i=0; i<strlen(hostname); i++) {
		if (hostname[i] == '.') {
			sprintf(dns_format, "%s%c", dns_format, i-j);
			strncat(dns_format, hostname+j, i-j);
			j = ++i;
		}
	}
	sprintf(dns_format, "%s%c", dns_format, i-j);
	strncat(dns_format, hostname+j, i-j);
}

