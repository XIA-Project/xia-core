/*
 * Author: Onha Choe
 * XIA specific BIND RR type
 */

#ifndef GENERIC_XIA_65532_H
#define GENERIC_XIA_65532_H 1
#define XID_KEYSIZE 40

typedef struct dns_rdata_xia {
        dns_rdatacommon_t common;
	int listsize;
} dns_rdata_xia_t;

#endif /* GENERIC_XIA_65532_H */
