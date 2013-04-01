
#ifndef CLICK_XIAXIONROUTETABLE_HH
#define CLICK_XIAXIONROUTETABLE_HH

#include <click/config.h>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include <click/xiaheader.hh>
#include <algorithm>
#include "scion/scionbeacon.hh"
#include "scion/define.hh"
#include "scion/config.hh"

#define RUN_MODE_NOTREADY 0
#define RUN_MODE_AD 1
#define RUN_MODE_HOST 2

CLICK_DECLS

class XIAXIONRouteTable: public Element {
  public:
    XIAXIONRouteTable();
    ~XIAXIONRouteTable();

    const char *class_name() const		{ return "XIAXIONRouteTable"; }
    const char *port_count() const		{ return "-/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    void push(int in_ether_port, Packet *);

    void add_handlers();
    static int initialize_handler(const String &, Element *, void *, ErrorHandler *);
    static int add_port2ifid_handler(const String &, Element *, void *, ErrorHandler *);

    int run_mode;
    uint8_t ofg_master_key[16];
    int host_mode_default_forward_port;
    std::map<int, int> port2ifid;

  private:
    int extract_xion_path(Packet *, uint8_t *);
    void modify_xion_path(Packet *, int, uint8_t *);

    bool init_ofg_key();
    bool update_ofg_key();
    int forwardDataPacket(int, uint8_t *);
    int normalForward(uint8_t, int, uint8_t *, uint8_t);
    int crossoverForward(uint8_t, uint8_t, int, uint8_t *, uint8_t);
    int verifyOF(int, uint8_t *);
    int verifyInterface(int, uint8_t *, uint8_t dir);
    bool getOfgKey(uint32_t timestamp, aes_context &);

    map<uint32_t, aes_context *> ofg_aes_ctx;
    ofgKey curr_ofg_key;
    ofgKey prev_ofg_key;
};

CLICK_ENDDECLS
#endif
