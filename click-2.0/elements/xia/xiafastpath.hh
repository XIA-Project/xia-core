#ifndef CLICK_FASTPATH_HH
#define CLICK_FASTPATH_HH
#include <click/element.hh>

CLICK_DECLS

#define KEYSIZE 20
#define ASSOCIATIVITY 8

struct item {
    uint8_t key[KEYSIZE];
    uint8_t port;
};

struct bucket {
    struct item item[ASSOCIATIVITY];
    uint8_t counter[ASSOCIATIVITY];
};

class XIAFastPath : public Element { public:
    XIAFastPath();
    ~XIAFastPath();
    const char *class_name() const      { return "XIAFastPath"; }
    const char *port_count() const      { return "n/n"; }
    const char *processing() const      { return PUSH; }
    void push(int, Packet *);
    int configure(Vector<String> &conf, ErrorHandler *errh);
   
    int initialize(ErrorHandler *);

    private:
    struct bucket* _bucket;
    uint32_t _bucket_size;
    uint32_t _offset;
    
    void update_cacheline(struct bucket *buck,  const uint8_t *key, int port);
    const uint8_t *getkey(Packet *p);
    int lookup(struct bucket *buck,  const uint8_t *key);
};
CLICK_ENDDECLS
#endif
