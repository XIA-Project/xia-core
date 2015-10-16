#ifndef CLICK_XIANEWXIDROUTETABLE_HH
#define CLICK_XIANEWXIDROUTETABLE_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/xiapath.hh>
#include "xcmp.hh"
#include "xiaxidroutetable.hh"
#include "scion.h"
#include "aesni.h"
CLICK_DECLS

#define RTE_LOGTYPE_HSR RTE_LOGTYPE_USER2
//#define RTE_LOG_LEVEL RTE_LOG_INFO
#define RTE_LOG_LEVEL RTE_LOG_DEBUG
#define VERIFY_OF

#define INGRESS_IF(HOF)                                                        \
  (ntohl((HOF)->ingress_egress_if) >>                                          \
   (12 +                                                                       \
    8)) // 12bit is  egress if and 8 bit gap between uint32 and 24bit field
#define EGRESS_IF(HOF) ((ntohl((HOF)->ingress_egress_if) >> 8) & 0x000fff)

#define LOCAL_NETWORK_ADDRESS IPv4(10, 56, 0, 0)
#define GET_EDGE_ROUTER_IPADDR(IFID)                                           \
  rte_cpu_to_be_32((LOCAL_NETWORK_ADDRESS | IFID))

#define MAX_NUM_ROUTER 16
#define MAX_NUM_BEACON_SERVERS 1
#define MAX_IFID 2 << 12

/*
=c
XIANEWXIDRouteTable(XID1 OUT1, XID2 OUT2, ..., - OUTn)

=s ip
simple XID routing table

=d
Routes XID according to a routing table.

=e

XIANEWXIDRouteTable(AD0 0, HID2 1, - 2)
It outputs AD0 packets to port 0, HID2 packets to port 1, and other packets to port 2.
If the packet has already arrived at the destination node, the packet will be destroyed,
so use the XIACheckDest element before using this element.

=a StaticIPLookup, IPRouteTable
*/

class XIANEWXIDRouteTable : public Element { public:

    XIANEWXIDRouteTable();
    ~XIANEWXIDRouteTable();

    const char *class_name() const		{ return "XIANEWXIDRouteTable"; }
    const char *port_count() const		{ return "-/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    void push(int in_ether_port, Packet *);

	int set_enabled(int e);
	int get_enabled();

protected:
    int lookup_route(int in_ether_port, Packet *);
    int process_xcmp_redirect(Packet *);

    static int set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int remove_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
	static String read_handler(Element *e, void *thunk);
	static int write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh);

    static String list_routes_handler(Element *e, void *thunk);

    //scion functions
    int scion_init(int port);
    int sync_interface(void); 
    // send a packet to neighbor AD router
    int send_egress(Packet *m, uint8_t dpdk_rx_port);

    int send_ingress(Packet *m, uint32_t next_ifid,
  			      uint8_t dpdk_rx_port);

    uint8_t get_type(SCIONHeader *hdr);
    void process_ifid_request(Packet *m, uint8_t dpdk_rx_port);
    void l2fwd_send_packet(Packet *m, uint8_t port);
    void process_pcb(Packet *m, uint8_t from_bs,
				      uint8_t dpdk_rx_port);

    void relay_cert_server_packet(Packet *m,
                                  uint8_t from_local_socket,
				  uint8_t dpdk_rx_port);

    void process_path_mgmt_packet(Packet *m,
                                  uint8_t from_local_ad,
				  uint8_t dpdk_rx_port);

    void deliver(Packet *m, uint32_t ptype,
				  uint8_t dpdk_rx_port);

    void forward_packet(Packet *m, uint32_t from_local_ad,
					 uint8_t dpdk_rx_port);
    void ingress_shortcut_xovr(Packet *m, uint8_t dpdk_rx_port);
    void ingress_peer_xovr(Packet *m, uint8_t dpdk_rx_port);
    void ingress_core_xovr(Packet *m, uint8_t dpdk_rx_port);
    void handle_ingress_xovr(Packet *m, uint8_t dpdk_rx_port);
    uint8_t verify_of(HopOpaqueField *hof, HopOpaqueField *prev_hof,
                                       uint32_t ts);
  void ingress_normal_forward(Packet *m, uint8_t dpdk_rx_port);
  void handle_egress_xovr(Packet *m, uint8_t dpdk_rx_port);
  void egress_shortcut_xovr(Packet *m, uint8_t dpdk_rx_port);
  void egress_peer_xovr(Packet *m, uint8_t dpdk_rx_port);
  void egress_core_xovr(Packet *m, uint8_t dpdk_rx_port);
  void egress_normal_forward(Packet *m,
			   uint8_t dpdk_rx_port);


private:
	HashTable<XID, XIARouteData*> _rts;
	XIARouteData _rtdata;
    uint32_t _drops;

	int _principal_type_enabled;
    int _num_ports;
    XIAPath _local_addr;
    XID _local_hid;
    XID _bcast_xid;

uint32_t beacon_servers[MAX_NUM_BEACON_SERVERS];
uint32_t certificate_servers[10];
uint32_t path_servers[10];

struct port_map {
  uint8_t egress;
  uint8_t local;
} port_map[16];

uint32_t neighbor_ad_router_ip[16];

uint32_t my_ifid[16]; // the current router's IFID

  struct keystruct rk; // AES-NI key structure


};

CLICK_ENDDECLS
#endif
