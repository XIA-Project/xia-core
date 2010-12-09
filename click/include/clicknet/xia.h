/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_XIA_H
#define CLICKNET_XIA_H

/*
 * <clicknet/xia.h> -- XIA packet header, only works in user-level click
 * 
 */

#define UNDEFINED_TYPE 0
#define AD_TYPE 1
#define HID_TYPE 2
#define CID_TYPE 3
#define SID_TYPE 4

struct click_xid_v1 {
    uint16_t    type;  /* AD, HID, CID, SID, etc */
    uint8_t	xid[20];
};

struct click_xia {
    uint8_t ver;
    uint8_t nexthdr;   /* next header */
    uint8_t niter;     /* total number of iterations */
    uint8_t nxid;      /* totoal number of XIDs */
    uint8_t intent;    /* final intent (is it always same as niter?) */
    uint8_t next;      /* next destination */
    uint8_t next_fb;      /* next fallback destination */
    uint8_t hlim;         /* hop limit */
    uint16_t payload_len;   /* payload length */ 

    uint8_t rest[0];     /* offset and xids */

    uint8_t * fb_offset() { return rest; }; 
    const uint8_t * fb_offset() const { return rest; }; 
    
    uint8_t fb_offset(int iterk) { return fb_offset()[iterk]; };
    const uint8_t fb_offset(int iterk) const { return fb_offset()[iterk]; };

    struct click_xid_v1 * xid()  { return (click_xid_v1 * )(rest+ niter);}; // first xid comes after offsets
    struct click_xid_v1 * dest()  { return  &(xid()[1]);}; // destination
    struct click_xid_v1 * src_stack() { return &(xid()[niter+1]); };   // first xid of the source stack comes after dest
    struct click_xid_v1 * src()  { return (click_xid_v1 * )(rest+ niter);}; // src xid is the first xid
    struct click_xid_v1 * fallback(int iterk) { return &(xid()[fb_offset(iterk)]);}; // fallback comes after src stack

    struct click_xid_v1 * xid() const { return (click_xid_v1 * )(rest+ niter);}; // first xid comes after offsets
    struct click_xid_v1 * dest() const { return  &(xid()[1]);}; // destination
    struct click_xid_v1 * src_stack() const { return &(xid()[niter+1]); };   // first xid of the source stack comes after dest
    struct click_xid_v1 * src() const { return (click_xid_v1 * )(rest+ niter);}; // src xid is the first xid
    const struct click_xid_v1 * fallback(int iterk) const { return &(xid()[fb_offset(iterk)]);}; // fallback comes after src stack
    int len() const { return niter + sizeof (click_xia) + sizeof(click_xid_v1)*nxid; };
};

#define FALLBACK_INVALID			0xFFU
#define XIA_V1	                		0x01U
#define NEXT_FB_UNDEFINED                       0xFFU


#endif
