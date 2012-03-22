#ifndef __XHCP_H__
#define __XHCP_H__

#define XHCP_SERVER_BEACON_INTERVAL 1
#define XHCP_CLIENT_ADEXPIRE_INTERVAL 15
#define XHCP_CLIENT_ADVERTISE_INTERVAL 3

#define XHCP_MAX_PACKET_SIZE 1024
#define XHCP_MAX_DAG_LENGTH 1024

#define HELLO 0
#define LSA 1
#define HOST_REGISTER 2

#define MAX_XID_SIZE 100

#define BHID "HID:1111111111111111111111111111111111111111"
#define SID_XHCP "SID:1110000000000000000000000000000000001111"
#define SID_XROUTE "SID:1110000000000000000000000000000000001112"

#define XHCP_TYPE_AD 1
#define XHCP_TYPE_GATEWAY_ROUTER_HID 2

typedef struct xhcp_pkt_entry {
	short type;
	char data[];
} xhcp_pkt_entry;

typedef struct xhcp_pkt {
	unsigned long seq_num;
	unsigned short num_entries;
	char data[];
} xhcp_pkt;

typedef struct advertise_info {
	char *selfdag;
	char *gwdag;
} advertise_info;

#endif
