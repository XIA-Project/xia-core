/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_XIA_H
#define CLICKNET_XIA_H

#include <stdint.h>

/*
 * <clicknet/xia.h> -- XIA packet header, only works in user-level click
 *
 */

// default principal types
#define CLICK_XIA_XID_TYPE_UNDEF    (0)
#define CLICK_XIA_XID_TYPE_AD       (0x10)
#define CLICK_XIA_XID_TYPE_HID      (0x11)
#define CLICK_XIA_XID_TYPE_CID      (0x12)
#define CLICK_XIA_XID_TYPE_SID      (0x13)
#define CLICK_XIA_XID_TYPE_IP       (0x14)
// BU principal types
// #define CLICK_XIA_XID_TYPE_I4ID     (0x15)
// #define CLICK_XIA_XID_TYPE_U4ID     (0x16)
// #define CLICK_XIA_XID_TYPE_XDP      (0x17)
// #define CLICK_XIA_XID_TYPE_SRVCID   (0x18)
// #define CLICK_XIA_XID_TYPE_FLOWID   (0x19)
// #define CLICK_XIA_XID_TYPE_ZF       (0x20)

// CMU principal types
#define CLICK_XIA_XID_TYPE_FID      (0x30)
#define CLICK_XIA_XID_TYPE_NCID     (0x31)
#define CLICK_XIA_XID_TYPE_ICID     (0x32)
#define CLICK_XIA_XID_TYPE_AID      (0x33)

#define CLICK_XIA_XID_TYPE_DUMMY    (0xff)

#define CLICK_XIA_XID_ID_LEN        20

#define CLICK_XIA_ADDR_MAX_NODES	20

#define CLICK_XIA_MAX_XID_TYPE_STR	8

#define XIA_XID_STR_SIZE ((CLICK_XIA_XID_ID_LEN*2)+CLICK_XIA_MAX_XID_TYPE_STR)
#define XIA_MAX_DAG_STR_SIZE (XIA_XID_STR_SIZE)*(CLICK_XIA_ADDR_MAX_NODES)

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
    struct click_xia_xid xid;
    struct click_xia_xid_edge edge[CLICK_XIA_XID_EDGE_NUM];
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
    struct click_xia_xid_node node[0];         /* XID node list */
};

typedef struct click_xia_xid xid_t;

typedef struct click_xia_xid_node node_t;

typedef struct {
    unsigned char s_count;
    node_t        s_addr[CLICK_XIA_ADDR_MAX_NODES];
} x_addr_t;



typedef struct {
    // common sockaddr fields
#ifdef __APPLE__
    unsigned char sx_len; // not actually large enough for sizeof(sockaddr_x)
    unsigned char sx_family;
#else
    unsigned short sx_family;
#endif

    // XIA specific fields
    x_addr_t      sx_addr;
} sockaddr_x;

#define CLICK_XIA_NXT_DATA		0
#define CLICK_XIA_NXT_XCMP		0x01
#define CLICK_XIA_NXT_XDGRAM	0x02
#define CLICK_XIA_NXT_XSTREAM	0x03
#define CLICK_XIA_NXT_FID       0x04
#define CLICK_XIA_NXT_SECRET    0x05
#define CLICK_XIA_NXT_QUIC      0x06


// XIA extension header
#pragma pack(push)
#pragma pack(1)
struct click_xia_ext {
    uint8_t nxt;     /* next header */
    uint8_t hlen;     /* header length (not payload length) */
    uint8_t type;     /* type of packet (TEMPORARY HACK!) */
    int8_t padding;  /* pad header to 4 bytes */
    uint8_t data[0];  /* extension data */
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
#define	  XCMP_UNREACH_INTENT		3	/*   bad port		     */
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
#define	  XCMP_UNREACH_UNSPECIFIED  16      /*    dag parsing error	     */
#define	XCMP_REDIRECT		5		/* shorter route, codes:     */
#define	  XCMP_REDIRECT_NET		0	/*   for network	     */
#define	  XCMP_REDIRECT_HOST		1	/*   for host		     */
#define	  XCMP_REDIRECT_TOSNET		2	/*   for tos and net	     */
#define	  XCMP_REDIRECT_TOSHOST		3	/*   for tos and host	     */
#define	XCMP_ECHO		8		/* echo service		     */
#define	XCMP_TIMXCEED		11		/* time exceeded, code:	     */
#define	  XCMP_TIMXCEED_TRANSIT		0	/*   ttl==0 in transit	     */
#define	  XCMP_TIMXCEED_REASSEMBLY	1	/*   ttl==0 in reassembly    */


#define BFID "FID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"

// changed from 0xff to 0xfe for BU compatability
#define LAST_NODE_DEFAULT	0x7e

#define HLIM_DEFAULT		250
#endif
