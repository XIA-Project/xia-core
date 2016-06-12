#ifndef CLICKNET_XTCP_H
#define CLICKNET_XTCP_H

#define XTCP_OPTIONS_MAX	256	// maximum size of the options block

// IMPORTANT!!! If accessing the header fields directly make sure to use the appropriate htonx and ntohx functions!
// the header fields should always be in network byte order
#pragma pack(push)
#pragma pack(1)
struct xtcp {
	uint8_t  th_nxt;  // keeping the same name as the other headers for consistancy, but is kinda ugly
	uint8_t  th_off;
	uint16_t th_flags;
	uint32_t th_seq;
	uint32_t th_ack;
	uint32_t th_win;

	uint8_t  data[0];
};
#pragma pack(pop)

#define XTH_FIN      0x0001
#define XTH_SYN      0x0002
#define XTH_RST      0x0004
#define XTH_PUSH     0x0008
#define XTH_ACK      0x0010
//#define XTH_URG      0x0020
#define XTH_ECE      0x0040
#define XTH_CWR      0x0080
#define XTH_NS       0x0100	// in flags2 in real tcp header

#endif
