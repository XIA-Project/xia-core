#include <click/config.h>
#include "xiafastpath.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAFastPath::XIAFastPath() :_bucket(0), _offset(0)
{
}

XIAFastPath::~XIAFastPath()
{
    if (_bucket) delete[] _bucket;
}

int
XIAFastPath::initialize(ErrorHandler *)
{
    _bucket = new struct bucket[_bucket_size];
    memset(_bucket, 0, _bucket_size * sizeof(struct bucket));
    return 0;
}

const char * XIAFastPath::getkey(Packet *p)
{
    return XIAHeader(p).payload()+ _offset;
}

void XIAFastPath::update_cacheline(struct bucket *buck,  const char * key, int port)
{
    int empty = 0;
    u8 max_counter = -1;
    for (int i=0;i<KEYSIZE;i++) {
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

int XIAFastPath::lookup(struct bucket *buck,  const char * key)
{
    int port = 0;
    for (int i=0;i<KEYSIZE;i++) {
	if (memcmp(key, buck->item[i].key , KEYSIZE)==0) {
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
                   "BUCKET_SIZE", 0, cpInteger, &bucket_size,
                   cpEnd) < 0)
        return -1;
 
   _bucket_size = bucket_size;  
   _offset = offset;
}

void XIAFastPath::push(int port, Packet * p)
{
    const char * key = getkey(p);
    uint32_t index = *(uint32_t*)key % _bucket_size;

    if (port==0) {
	int outport = lookup(&_bucket[index], key);
	output(outport).push(p);
    } else {
	/* Cache result */
	update_cacheline(&_bucket[index], key, port);
	output(port).push(p);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAEncap)
ELEMENT_MT_SAFE(XIAEncap)
