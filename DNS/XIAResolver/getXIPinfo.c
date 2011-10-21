#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>

#include "getXIPinfo.h"

/*
 * get xia destination DAG
 * returns
 * >0 successful, number of RR's returned
 * -1 no entry found [DNS_NOENTRY]
 * -2 connection failure [DNS_FAILURE]
 */
int getXIPinfo(XIP_info_t **results, char *host) {
	int dns_sock;
	struct sockaddr_in dns_addr;
	char dns_serv[256];
	dns_mh_t query_header;
	dns_mh_t *response_header;
	dns_mq_t query_question;
	char query_buf[1024], response_buf[1024];
	char *query_buf_offset = query_buf;
	char *response_buf_offset = response_buf;
	socklen_t dns_addr_len;
	int i;
	char line[512];
	char *linend;
	XIP_info_t *answer;

	// first look at /etc/hosts_xia for possible entry
	FILE *hostsfp = fopen(ETC_HOSTS, "r");
	int answer_cnt=0;
	if (hostsfp != NULL) {
		while (fgets(line, 511, hostsfp) != NULL) {
			linend = line+strlen(line)-1;
			while (*linend == '\r' || *linend == '\n' || *linend == '\0') {
				linend--; 
			}
			*(linend+1) = '\0';
			if (line[0] == '#') {
				continue;
			} else if (!strncmp(line, host, strlen(host))
					   && line[strlen(host)] == ' ') {
				answer = malloc(sizeof(XIP_info_t));
				strncpy(answer->dag, line+strlen(host)+1, strlen(line)-strlen(host)-1);
				answer->dag[strlen(line)-strlen(host)-1] = '\0';
				answer->next = *results;
				*results = answer;
				answer_cnt++;
			}
		}
		fclose(hostsfp);
		if (answer_cnt > 0) {
			return answer_cnt;
		}
	}

	// get dns list from /etc/resolv.xiaconf
	FILE *resolvfp = fopen(RESOLV_CONF, "r");
	char nameserver[256];
	if (resolvfp == NULL) {
		return DNS_FAILURE;
	} else {
		while (fgets(line, 255, resolvfp ) != NULL) {
			linend = line+strlen(line)-1;
			while (*linend == '\r' || *linend == '\n' || *linend == '\0') {
				linend--; 
			}
			*(linend+1) = '\0';
			if (line[0] == '#') {
				continue;
			} else if (!strncmp(line, "nameserver ", 11)) {
				strncpy(nameserver, line+11, strlen(line)-11);
				nameserver[strlen(line)-11] = '\0';
				break;
			}
		}
	}
	sprintf(dns_serv, "%s", nameserver);
	fclose(resolvfp);

	// dns server connection
	dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	dns_addr.sin_family = AF_INET;
	dns_addr.sin_port = htons(DNS_DEF_PORT);
	dns_addr.sin_addr.s_addr = inet_addr(dns_serv);

	// init dns query header
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

	// init dns query question
	query_question.qname = malloc(strlen(host)+2);
	memset(query_question.qname, 0, strlen(host)+2);
	nameconv_dnstonorm(query_question.qname, host);
	query_question.qtype = htons(RR_XIA);
	query_question.qclass = htons(1);

	// create query buffer
	memset(query_buf, 0, 1024);
	memcpy(query_buf_offset, &query_header, sizeof(query_header));
	query_buf_offset += sizeof(query_header);
	memcpy(query_buf_offset, query_question.qname, strlen(query_question.qname)+1);
	query_buf_offset += strlen(query_question.qname)+1;
	free(query_question.qname);
	memcpy(query_buf_offset, &query_question.qtype, sizeof(short));
	query_buf_offset += sizeof(short);
	memcpy(query_buf_offset, &query_question.qclass, sizeof(short));
	query_buf_offset += sizeof(short);

	// send query to dns server
	if (sendto(dns_sock, query_buf, query_buf_offset-query_buf, 0,
				(struct sockaddr *) &dns_addr, sizeof(dns_addr)) == -1) {
		return DNS_FAILURE;
	}
	// receive response from dns server
	if (recvfrom(dns_sock, response_buf, 1024, 0,
				(struct sockaddr *) &dns_addr, &dns_addr_len) == -1) {
		return DNS_FAILURE;
	}

	// parse out response header data
	response_buf_offset = response_buf;
	response_header = (dns_mh_t *)response_buf_offset;
	response_buf_offset += sizeof(dns_mh_t);

	// no answer found
	if (ntohs(response_header->ancount) == 0) {
		return DNS_NOENTRY;
	}

	response_buf_offset += strlen(response_buf_offset)+1;
	response_buf_offset += 2*sizeof(short);

	// store each XIP info as XIP_info_t linked list
	for (i=0; i<ntohs(response_header->ancount); i++) {
		answer = malloc(sizeof(XIP_info_t));
		short rdlength;
		// offset according to name format
		// case 1: pointer
		if ((ntohs(*(unsigned short *)response_buf_offset) & 3<<14) == 3<<14) {
			response_buf_offset += sizeof(short);
		}
		// case 2: naive name in dns format
		else {
			response_buf_offset += strlen(response_buf_offset)+1;
		}
		// offset accounted for type, class, TTL
		response_buf_offset += 4*sizeof(short);
		rdlength = ntohs(*((unsigned short *)(response_buf_offset)));
		response_buf_offset += sizeof(short);
		memcpy(answer->dag, response_buf_offset, rdlength);
		answer->dag[rdlength] = '\0';
		answer->next = *results;
		*results = answer;
	}

	return ntohs(response_header->ancount);
}

/*
 * free XIP_info_t data structure allocated by getXIPinfo()
 */
void freeXIPinfo(XIP_info_t **result) {
	XIP_info_t *record, *tmp;
	record = *result;
	while (record != NULL) {
		tmp = record->next;
		free(record);
		record = tmp;
	}
	*result = NULL;
}

void nameconv_dnstonorm(char *dst, char *src) {
	int i, j;
	j=0;
	for (i=0; i<strlen(src); i++) {
		if (src[i] == '.') {
			sprintf(dst, "%s%c", dst, i-j);
			strncat(dst, src+j, i-j);
			j = ++i;
		}
	}
	sprintf(dst, "%s%c", dst, i-j);
	strncat(dst, src+j, i-j);
}
