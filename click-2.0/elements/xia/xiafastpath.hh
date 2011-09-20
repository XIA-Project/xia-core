#ifndef CLICK_FASTPATH_HH
#define CLICK_FASTPATH_HH
#include <click/element.hh>


#define KEYSIZE 20
#define ASSOCIATIVITY 2

struct item {
    uint8_t key[KEYSIZE];
    uint8_t port;
};

struct bucket {
    struct item item[ASSOCIATIVITY];
    uint8_t counter[ASSOCIATIVITY];
} 
#if CLICK_LINUXMODULE
 ____cacheline_aligned_in_smp;
#else
  __attribute__ ((aligned (64)));
#endif


CLICK_DECLS
class XIAFastPath : public Element { public:
    XIAFastPath();
    ~XIAFastPath();
    const char *class_name() const      { return "XIAFastPath"; }
    const char *port_count() const      { return "-/-"; }
    const char *processing() const      { return PUSH; }
    void push(int, Packet *);
    int configure(Vector<String> &conf, ErrorHandler *errh);
   
    int initialize(ErrorHandler *);

    private:
    struct bucket* _buckets[NUM_CLICK_CPUS];
    uint32_t _bucket_size;
    int32_t _offset;
    
    void update_cacheline(struct bucket *buck,  const uint8_t *key, int port);
    const uint8_t *getkey(Packet *p);
    int lookup(struct bucket *buck,  const uint8_t *key);
};
CLICK_ENDDECLS
#endif
