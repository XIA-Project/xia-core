/*
 * xiafastpath.{cc,hh} -- a fast-path implementation for XIA packets
 */
#include <click/config.h>
#include "xiafastpath.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <clicknet/xia.h>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAFastPath::XIAFastPath() : _offset(0)
{
    memset(_buckets, 0, sizeof(struct bucket*) * NUM_CLICK_CPUS);
}

XIAFastPath::~XIAFastPath()
{
    for (int i = 0; i < NUM_CLICK_CPUS; i++)
		if (_buckets[i])
			delete[] _buckets[i];
}

int
XIAFastPath::initialize(ErrorHandler *)
{
    for (int i = 0; i < NUM_CLICK_CPUS; i++) {
        _buckets[i] = new struct bucket[_bucket_size];
        memset(_buckets[i], 0, _bucket_size * sizeof(struct bucket));
    }
    return 0;
}

const uint8_t* XIAFastPath::getkey(Packet *p)
{
    return XIAHeader(p).payload() + _offset;
}

void XIAFastPath::update_cacheline(struct bucket *buck, const uint8_t *key, int port)
{
    int empty = 0;
    uint8_t max_counter = -1;
    for (int i = 0; i < ASSOCIATIVITY; i++) {
		if (max_counter< buck->counter[i]) {
			max_counter = buck->counter[i];
			empty = i;
		}
		buck->counter[i]++;
    }
    buck->counter[empty] = 0;
    memcpy(buck->item[empty].key, key, KEYSIZE);
    buck->item[empty].port = port;
}

int XIAFastPath::lookup(struct bucket *buck,  const uint8_t *key)
{
    int port = 0;
    for (int i = 0;i < ASSOCIATIVITY; i++) {
	if (memcmp(key, buck->item[i].key, KEYSIZE) == 0) {
	    port = buck->item[i].port;
	    buck->counter[i]=0;
	} else 
	    buck->counter[i]++;
   }
   return port; 
}

int XIAFastPath::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int offset = 0;
    int bucket_size = 0;
    if (cp_va_kparse(conf, this, errh,
                   "KEY_OFFSET", 0, cpInteger, &offset,
                   "BUCKET_SIZE", cpkM, cpInteger, &bucket_size,
                   cpEnd) < 0)
        return -1;
 
   _bucket_size = bucket_size;  
   _offset = offset;
   return 0;
}

void XIAFastPath::push(int port, Packet * p)
{
#if HAVE_MULTITHREAD
#if CLICK_USERLEVEL
    int thread_id = click_current_thread_id;
#else
    int thread_id = click_current_processor();
#endif
#else
    int thread_id = 0;
#endif
    const uint8_t *key = getkey(p);
 
    uint32_t index = *(uint32_t*)key % _bucket_size;
    //click_chatter("fastpath_key %02x%02x%02x%02x, bucket_index %u", key[0], key[1], key[2],  key[3], index);

    if (port==0) {
		int outport = lookup(&_buckets[thread_id][index], key);
		output(outport).push(p);
    } else {
		// Cache result
		update_cacheline(&_buckets[thread_id][index], key, port);
		output(port).push(p);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAFastPath)
ELEMENT_MT_SAFE(XIAFastPath)
