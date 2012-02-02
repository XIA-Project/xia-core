#ifndef CLICK_IPFASTPATH_HH
#define CLICK_IPFASTPATH_HH
#include <click/element.hh>

CLICK_DECLS
#define IP_ASSOCIATIVITY 1

struct iplookup_result {
    uint32_t key;
    uint8_t port;
};

struct ipfp_bucket {
    struct iplookup_result item[IP_ASSOCIATIVITY];
    uint8_t counter[IP_ASSOCIATIVITY];
};

class IPFastPath : public Element { public:
    IPFastPath();
    ~IPFastPath();
    const char *class_name() const      { return "IPFastPath"; }
    const char *port_count() const      { return "-/-"; }
    const char *processing() const      { return PUSH; }
    void push(int, Packet *);
    int configure(Vector<String> &conf, ErrorHandler *errh);
   
    int initialize(ErrorHandler *);

    private:
    struct ipfp_bucket* _bucket;
    uint32_t _bucket_size;
    
    void update_cacheline(struct ipfp_bucket *buck,  const uint32_t ipv4_dst, int port);
    int lookup(struct ipfp_bucket *buck,  const uint32_t ipv4_dst);
};
CLICK_ENDDECLS
#endif
