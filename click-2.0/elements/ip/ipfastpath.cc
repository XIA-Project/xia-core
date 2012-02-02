#include <click/config.h>
#include "ipfastpath.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/nameinfo.hh>
#include <click/confparse.hh>

CLICK_DECLS

IPFastPath::IPFastPath() :_bucket(0)
{
}

IPFastPath::~IPFastPath()
{
    if (_bucket) delete[] _bucket;
}

int
IPFastPath::initialize(ErrorHandler *)
{
    _bucket = new struct ipfp_bucket[_bucket_size];
    memset(_bucket, 0, _bucket_size * sizeof(struct ipfp_bucket));
    return 0;
}

void IPFastPath::update_cacheline(struct ipfp_bucket *buck,  const uint32_t ipv4_dst, int port)
{
    int empty = 0;
    uint8_t max_counter = -1;
    for (int i=0;i<IP_ASSOCIATIVITY;i++) {
	if (max_counter< buck->counter[i]) {
	    max_counter = buck->counter[i];
	    empty = i;
	}
	buck->counter[i]++;
    }
    buck->counter[empty] = 0;
    buck->item[empty].key = ipv4_dst;
    buck->item[empty].port = port;
}

int IPFastPath::lookup(struct ipfp_bucket *buck,  const uint32_t ipv4_dst)
{
    int port = 0;
    for (int i=0;i<IP_ASSOCIATIVITY;i++) {
	if (buck->item[i].key== ipv4_dst) {
	    port = buck->item[i].port;
	    buck->counter[i]=0;
	} else 
	    buck->counter[i]++;
   }
   return port; 
}

int IPFastPath::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int bucket_size = 0;
    if (cp_va_kparse(conf, this, errh,
                   "BUCKET_SIZE", cpkM, cpInteger, &bucket_size,
                   cpEnd) < 0)
        return -1;
 
   _bucket_size = bucket_size;  
   return 0;
}

void IPFastPath::push(int port, Packet * p)
{
    const click_ip *iph  = p->ip_header();
    const uint32_t key = iph->ip_dst.s_addr;
    uint32_t index = key % _bucket_size;

    if (port==0) {
        /* Fastpath lookup */ 
	int outport = lookup(&_bucket[index], key);
	output(outport).push(p);
    } else {
	/* Cache result */
	update_cacheline(&_bucket[index], key, port);
	output(port).push(p);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPFastPath)
