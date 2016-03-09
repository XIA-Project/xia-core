/* 
 * SCION HEADER 
 */

#ifndef _SCION_H_
#define _SCION_H_
#define SCIOND_API_HOST "127.255.255.254"
#define SCIOND_API_PORT 3333

#define MAX_HOST_ADDR_LEN 8

#define NORMAL_OF    0x0
#define LAST_OF      0x10
#define PEER_XOVR    0x8
#define TDC_XOVR     0x40
#define NON_TDC_XOVR 0x60
#define INPATH_XOVR  0x70
#define INTRATD_PEER 0x78
#define INTERTD_PEER 0x7C

#define NORMAL_OF 0x0
#define XOVR_POINT 0x10

#define SCION_ISD_AD_LEN 4
#define SCION_ADDR_LEN 4
#define MAX_PATH_LEN 512
#define MAX_TOTAL_PATHS 20

#define REV_TOKEN_LEN 32

#define IS_HOP_OF(x) !((x) & 0x80)

/* ===========  packetheader.h ============= */
#pragma pack(push)
#pragma pack(1)

typedef struct {
    int addrLen;
    uint8_t addr[MAX_HOST_ADDR_LEN];
    uint16_t port;
} HostAddr;

typedef struct{
    uint32_t isd_ad;
    HostAddr host;
} SCIONAddr;


#define ISD(isd_ad) (isd_ad >> 20)
#define AD(isd_ad) (isd_ad & 0xfffff)
#define ISD_AD(isd, ad) ((isd) << 20 | (ad))

#define SCION_ADDR_PAD 8
 
typedef struct {
  /** Packet Type of the packet (version, srcType, dstType) */
  uint16_t versionSrcDst;
  /** Total Length of the packet */
  uint16_t totalLen;
	/** Index of current Info opaque field*/
	uint8_t currentIOF;
	/** Index of current opaque field*/
	uint8_t currentOF;
	/** next header type, shared with IP protocol number*/
	uint8_t nextHeader;
	/** Header length that includes the path */
	uint8_t headerLen;
} SCIONCommonHeader;

#define SRC_TYPE(sch) ((ntohs(sch->versionSrcDst) & 0xfc0) >> 6)
#define DST_TYPE(sch) (ntohs(sch->versionSrcDst) & 0x3f)

typedef struct {
    SCIONCommonHeader commonHeader;
    SCIONAddr srcAddr;
    //SCIONAddr dstAddr;  // length of srcAddr depends on its type, so we cannot get dstAddr by a pointer cast...
    //uint8_t *path;
} SCIONHeader;

typedef struct {
//	SCIONCommonHeader commonHeader;  //now IFID in on the SCION UDP
//	SCIONAddr srcAddr;
//	SCIONAddr dstAddr;
	uint16_t reply_id; // how many bits?
	uint16_t request_id; // how many bits?
} IFIDHeader;

typedef struct {
/**
    Opaque field for a hop in a path of the SCION packet header.
    Each hop opaque field has a info (8 bits), expiration time (8 bits)
    ingress/egress interfaces (2 * 12 bits) and a MAC (24 bits) authenticating
    the opaque field.
**/

	uint8_t info;
	uint8_t exp_type;
	//uint16_t ingress_if:12;
	//uint16_t egress_if:12;
	//uint32_t ingress_egress_if:24;
	//uint32_t mac : 24;	
        uint8_t ingress_egress[3];
        uint8_t mac[3];
} HopOpaqueField ;


/**
    @brief Special Opaque Field Structure
    The info opaque field contains type info of the path-segment (1 byte),
    a creation timestamp (4 bytes), the ISD ID (2 byte) and # hops for this
    segment (1 byte).

*/
typedef struct {
	/** Info field with timestamp information */
	uint8_t info;
	/** Timestamp value in 16 bit number */
	//uint32_t timestamp;
        uint8_t timestamp[4];
	/** TD Id */
	//uint16_t isd_id;
        uint8_t isd_id[2];
	/** Number of hops under this timestamp (either up or down)*/
	uint8_t hops;
} InfoOpaqueField;


typedef struct {
	uint16_t isd_id:12;
	uint32_t ad_id:20;
	HopOpaqueField hof;
	char ig_rev_token[REV_TOKEN_LEN];
} PCBMarking;

typedef struct {
	uint16_t cert_ver;
	uint16_t sig_len;
	uint16_t asd_len;
	uint16_t block_len;
	PCBMarking pcbm;
	PCBMarking* pms;
	char* asd;
	char eg_rev_token[REV_TOKEN_LEN];
	char* sig;
} ADMarking;

typedef struct {
	InfoOpaqueField iof;
	uint32_t trc_ver;
	uint16_t if_id;
	char segment_id[REV_TOKEN_LEN];
	ADMarking* ads;
} PathSegment;

typedef struct {
//	SCIONHeader hdr;  //now PCB is on the SCION UDP
	PathSegment payload;
} PathConstructionBeacon;

#define DATA_PACKET 0
// Path Construction Beacon
#define BEACON_PACKET 1
// Path management packet from/to PS
#define PATH_MGMT_PACKET 2
// TRC file request to parent AD
#define TRC_REQ_PACKET 3
// TRC file request to lCS
#define TRC_REQ_LOCAL_PACKET 4
// TRC file reply from parent AD
#define TRC_REP_PACKET 5
// cert chain request to parent AD
#define CERT_CHAIN_REQ_PACKET 6
// local cert chain request
#define CERT_CHAIN_REQ_LOCAL_PACKET 7
// cert chain reply from lCS
#define CERT_CHAIN_REP_PACKET 8
// IF ID packet to the peer router
#define IFID_PKT_PACKET 9
// error condition
#define PACKET_TYPE_ERROR 99

// SCION UDP packet classes
#define PCB_CLASS 0
#define IFID_CLASS 1
#define CERT_CLASS 2
#define PATH_CLASS 3

// IFID Packet types
#define IFID_PAYLOAD_TYPE 0

// PATH Packet types
#define PMT_REQUEST_TYPE 0
#define PMT_REPLY_TYPE 1
#define PMT_REG_TYPE 2
#define PMT_SYNC_TYPE 3
#define PMT_REVOCATION_TYPE 4
#define PMT_IFSTATE_INFO_TYPE 5
#define PMT_IFSTATE_REQ_TYPE 6

#pragma pack(pop)


//Address type
// Null address type
#define ADDR_NONE_TYPE  0
// IPv4 address type
#define ADDR_IPV4_TYPE  1
// IPv6 address type
#define ADDR_IPV6_TYPE  2
// SCION Service address type
#define ADDR_SVC_TYPE  3

// SVC addresses
#define SVC_BEACON 1
#define SVC_PATH_MGMT 2
#define SVC_CERT_MGMT 3
#define SVC_IFID 4

// L4 Protocols
#define L4_UDP 17

//DPDK port
#define DPDK_EGRESS_PORT 0
#define DPDK_LOCAL_PORT 1

//Types for HopOpaqueFields (7 MSB bits).
#define    HOP_NORMAL_OF  0b0000000
#define    HOP_XOVR_POINT  0b0010000  //Indicates a crossover point.
//Types for Info Opaque Fields (7 MSB bits).
#define    IOF_CORE  0b1000000
#define    IOF_SHORTCUT  0b1100000
#define    IOF_INTRA_ISD_PEER  0b1111000
#define    IOF_INTER_ISD_PEER  0b1111100

#define SCION_PACKET_SIZE 256
#define SCION_PACKET_PAYLOAD_LEN 16

#define SCION_PACKET_SIZE 256
#define SCION_PACKET_PAYLOAD_LEN 16

#define SCION_PACKET // enable SCION packet in XIA packet

#endif /* _ROUTE_H_ */
