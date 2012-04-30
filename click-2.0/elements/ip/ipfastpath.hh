#ifndef CLICK_IPFASTPATH_HH
#define CLICK_IPFASTPATH_HH
#include <click/element.hh>

/*
=c
IPFastPath()

=s ip
caches the forwarding result of IP addresses.

=e

*/

// the number of items per hash bucket
#define IP_ASSOCIATIVITY 2

struct iplookup_result {
    uint32_t key;
    uint8_t port;
};

struct ipfp_bucket {
    struct iplookup_result item[IP_ASSOCIATIVITY];
    uint8_t counter[IP_ASSOCIATIVITY];
}
#if CLICK_LINUXMODULE
____cacheline_aligned_in_smp;
#else
__attribute__ ((aligned (64)));
#endif

CLICK_DECLS
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
    struct ipfp_bucket* _buckets[NUM_CLICK_CPUS];
    uint32_t _bucket_size;
    
    void update_cacheline(struct ipfp_bucket *buck,  const uint32_t ipv4_dst, int port);
    int lookup(struct ipfp_bucket *buck,  const uint32_t ipv4_dst);
};
CLICK_ENDDECLS
#endif
