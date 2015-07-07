/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_XIA_H
#define CLICKNET_XIA_H

#include <stdint.h>

/*
 * <clicknet/xia.h> -- XIA packet header, only works in user-level click
 * 
 */

#define CLICK_XIA_XID_TYPE_UNDEF    (0)
#define CLICK_XIA_XID_TYPE_AD       (0x10)
#define CLICK_XIA_XID_TYPE_HID      (0x11)
#define CLICK_XIA_XID_TYPE_CID      (0x12)
#define CLICK_XIA_XID_TYPE_SID      (0x13)
#define CLICK_XIA_XID_TYPE_IP       (0x14)

#define CLICK_XIA_XID_ID_LEN        20

struct click_xia_xid {
    uint32_t type;
    uint8_t id[CLICK_XIA_XID_ID_LEN];
};

#pragma pack(push)
#pragma pack(1)     // without this, the size of the struct would not be 1 byte
struct click_xia_xid_edge
{
#if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    unsigned idx : 7;                   /* index of node this edge points to */
    unsigned visited : 1;               /* visited edge? */
#elif CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    unsigned visited : 1;
    unsigned idx : 7;
#else
#   error "unknown byte order"
#endif
};
#pragma pack(pop)

#define CLICK_XIA_XID_EDGE_NUM      4
#define CLICK_XIA_XID_EDGE_UNUSED   127u

struct click_xia_xid_node {
    click_xia_xid xid;
    click_xia_xid_edge edge[CLICK_XIA_XID_EDGE_NUM];
};

struct click_xia_common {
    uint8_t ver;
    uint8_t rest[0];
};

// XIA network layer packet header
struct click_xia {
    uint8_t ver;			/* header version */
    uint8_t nxt;			/* next header */
    uint16_t plen;			/* payload length */
    uint8_t hlim;			/* hop limit */
    uint8_t dnode;			/* total number of dest nodes */
    uint8_t snode;			/* total number of src nodes */
    //uint8_t dints;
    //uint8_t sints;
    int8_t last;			/* index of last visited node (note: integral) */
    click_xia_xid_node node[0];         /* XID node list */
};

#define CLICK_XIA_NXT_CID       12  /* CID-source specific key-value list */
#define CLICK_XIA_NXT_XCMP		61	/*  XCMP header */
#define CLICK_XIA_NXT_HDR_MAX   (CLICK_XIA_NXT_NO-1)  /* maximum non-upper-layer nxt value */
#define CLICK_XIA_NXT_NO        59                      /* no next header (as in IPv6) */
#define CLICK_XIA_NXT_TRN       60  /* Transport header */

// XIA extension header
#pragma pack(push)
#pragma pack(1)    
struct click_xia_ext {
    uint8_t nxt;			/* next header */
    uint8_t hlen;			/* header length (not payload length) */
    uint8_t data[0];                    /* extension data */
};

// XIA control message protocol header (followed by initial packet data)
struct click_xia_xcmp {
    uint8_t type;
    uint8_t code;
    uint16_t cksum;
    uint8_t rest[0];
};
#pragma pack(pop)

//#define CLICK_XIA_PROTO_XCMP 0
//// XIA control message protocol header for echo
//struct click_xia_xcmp_sequenced {
//    uint8_t type;
//    uint8_t code;
//    uint16_t identifier;
//    uint16_t sequence;
//    uint8_t rest[0];
//};
//
#define	XCMP_ECHOREPLY		0		/* echo reply		     */
#define	XCMP_UNREACH		3		/* dest unreachable, codes:  */
#define	  XCMP_UNREACH_NET		0	/*   bad net		     */
#define	  XCMP_UNREACH_HOST		1	/*   bad host		     */
#define	  XCMP_UNREACH_PROTOCOL		2	/*   bad protocol	     */
#define	  XCMP_UNREACH_PORT		3	/*   bad port		     */
#define	  XCMP_UNREACH_NEEDFRAG		4	/*   IP_DF caused drop	     */
#define	  XCMP_UNREACH_SRCFAIL		5	/*   src route failed	     */
#define	  XCMP_UNREACH_NET_UNKNOWN	6	/*   unknown net	     */
#define	  XCMP_UNREACH_HOST_UNKNOWN	7	/*   unknown host	     */
#define	  XCMP_UNREACH_ISOLATED		8	/*   src host isolated	     */
#define	  XCMP_UNREACH_NET_PROHIB	9	/*   net prohibited access   */
#define	  XCMP_UNREACH_HOST_PROHIB	10	/*   host prohibited access  */
#define	  XCMP_UNREACH_TOSNET		11	/*   bad tos for net	     */
#define	  XCMP_UNREACH_TOSHOST		12	/*   bad tos for host	     */
#define	  XCMP_UNREACH_FILTER_PROHIB	13	/*   admin prohib	     */
#define	  XCMP_UNREACH_HOST_PRECEDENCE	14	/*   host prec violation     */
#define	  XCMP_UNREACH_PRECEDENCE_CUTOFF 15	/*   prec cutoff	     */
#define	XCMP_REDIRECT		5		/* shorter route, codes:     */
#define	  XCMP_REDIRECT_NET		0	/*   for network	     */
#define	  XCMP_REDIRECT_HOST		1	/*   for host		     */
#define	  XCMP_REDIRECT_TOSNET		2	/*   for tos and net	     */
#define	  XCMP_REDIRECT_TOSHOST		3	/*   for tos and host	     */
#define	XCMP_ECHO		8		/* echo service		     */
#define	XCMP_TIMXCEED		11		/* time exceeded, code:	     */
#define	  XCMP_TIMXCEED_TRANSIT		0	/*   ttl==0 in transit	     */
#define	  XCMP_TIMXCEED_REASSEMBLY	1	/*   ttl==0 in reassembly    */

#define BHID "HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#endif
