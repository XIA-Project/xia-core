#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>

#include "Xsocket.h"
#include "XgetDAGbyname.h"


#define HID0 "HID:0000000000000000000000000000000000000000"
#define AD0 "AD:1000000000000000000000000000000000000000"
#define SID_BIND "SID:1110000000000000000000000000000000001114"


static char DAG[256];


/*
 * get xia destination DAG
 * returns
 * DAG if successful, NULL otherwise
 */
const char *XgetDAGbyname(char *name) {
	int dns_sock;
	char dns_serv[256];
	dns_mh_t query_header;
	dns_mh_t *response_header;
	dns_mq_t query_question;
	char query_buf[1024], response_buf[1024];
	char *query_buf_offset = query_buf;
	char *response_buf_offset = response_buf;
	char line[512];
	char *linend;
    char serv_addr[512];
    size_t serv_addr_len;

	// first look at /etc/hosts_xia for possible entry
	FILE *hostsfp = fopen(ETC_HOSTS, "r");
	int answer_found = 0;
	if (hostsfp != NULL) {
		while (fgets(line, 511, hostsfp) != NULL) {
			linend = line+strlen(line)-1;
			while (*linend == '\r' || *linend == '\n' || *linend == '\0') {
				linend--; 
			}
			*(linend+1) = '\0';
			if (line[0] == '#') {
				continue;
			} else if (!strncmp(line, name, strlen(name))
					   && line[strlen(name)] == ' ') {
				strncpy(DAG, line+strlen(name)+1, strlen(line)-strlen(name)-1);
				DAG[strlen(line)-strlen(name)-1] = '\0';
        answer_found = 1;
			}
		}
		fclose(hostsfp);
		if (answer_found) {
      return DAG;
		}
	}

	// get dns list from /etc/resolv.xiaconf
	FILE *resolvfp = fopen(RESOLV_CONF, "r");
	char nameserver[256];
	if (resolvfp == NULL) {
    return NULL;
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
    dns_sock = Xsocket(XSOCK_DGRAM);
    if (dns_sock < 0)
        return NULL;
    
    sprintf(serv_addr, "RE %s %s %s", AD0, HID0, SID_BIND);
    serv_addr_len = strlen(serv_addr) + 1;

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
	query_question.qname = malloc(strlen(name)+2);
	memset(query_question.qname, 0, strlen(name)+2);
	nameconv_dnstonorm(query_question.qname, name);
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
    int bytes_sent;
    if ((bytes_sent = Xsendto(dns_sock, query_buf, query_buf_offset - query_buf,
            0, serv_addr, serv_addr_len)) < 0) {
        return NULL;
    }
//printf("After Xsento to (%s): query=%s\n", serv_addr, query_buf);     
    // receive response from dns server
    if (Xrecvfrom(dns_sock, response_buf, 1024, 0, serv_addr,
            &serv_addr_len) < 0) {
		return NULL;
	}

	// parse out response header data
	response_buf_offset = response_buf;
	response_header = (dns_mh_t *)response_buf_offset;
	response_buf_offset += sizeof(dns_mh_t);

	// no answer found
	if (ntohs(response_header->ancount) == 0) {
		return NULL;
	}

	response_buf_offset += strlen(response_buf_offset)+1;
	response_buf_offset += 2*sizeof(short);

	// store each XIP info as XIP_info_t linked list
	if (ntohs(response_header->ancount)) {
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
		memcpy(DAG, response_buf_offset, rdlength);
		DAG[rdlength] = '\0';
    return DAG;
	}
  return NULL;
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
