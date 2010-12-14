/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_XIA_H
#define CLICKNET_XIA_H

/*
 * <clicknet/xia.h> -- XIA packet header, only works in user-level click
 * 
 */

#define CLICK_XIA_XID_TYPE_UNDEFINED 0
#define CLICK_XIA_XID_TYPE_AD 1
#define CLICK_XIA_XID_TYPE_HID 2
#define CLICK_XIA_XID_TYPE_CID 3
#define CLICK_XIA_XID_TYPE_SID 4

#define CLICK_XIA_XID_ADDR_LEN 20

struct click_xia_xid {
    uint16_t type;
    uint8_t addr[CLICK_XIA_XID_ADDR_LEN];
};

struct click_xia_xid_node {
    click_xia_xid xid;
    uint8_t incr;
#if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    unsigned reserved : 7;
    unsigned visited : 1;
#elif CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    unsigned visited : 1;
    unsigned reserved : 7;
#else
#   error "unknown byte order"
#endif
};

struct click_xia_common {
    uint8_t ver;
    uint8_t rest[0];
};

struct click_xia {
    uint8_t ver;			/* header version */
    uint8_t nxt;			/* next header */
    uint16_t plen;			/* payload length */
    uint8_t nxids;			/* total number of all XIDs */
    uint8_t ndst;			/* number of destination path XIDs
                                           i.e. start index of source path XIDs */
    uint8_t last;			/* index of the last visited XID */
    uint8_t hlim;			/* hop limit */
    uint8_t flags;			/* flags */
    click_xia_xid_node node[0];         /* XID node list */
};

#endif
