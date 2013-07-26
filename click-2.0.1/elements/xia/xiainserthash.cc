/*
 * xiainserthash.{cc,hh} -- inserts a hash of the partial DAG address in the payload
 */

#include <click/config.h>
#include "xiainserthash.hh"
#include "xiafastpath.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/string.hh>
#include <click/packet.hh>
#include <click/xiaheader.hh>
#include "elements/ipsec/sha1_impl.hh"
CLICK_DECLS

XIAInsertHash::XIAInsertHash() 
{
}

XIAInsertHash::~XIAInsertHash() 
{
}

int XIAInsertHash::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int offset = 0;
    if (cp_va_kparse(conf, this, errh,
                   "KEY_OFFSET", 0, cpInteger, &offset,
                   cpEnd) < 0)
        return -1;
 
   _offset = offset;
   return 0;
}

Packet * XIAInsertHash::simple_action(Packet *p)
{
    SHA1_ctx sha_ctx;
    unsigned char digest[KEYSIZE];
    WritableXIAHeader hdr(p->uniqueify());
    int hdr_size = hdr.hdr_size();

    // enough space in packt?
    assert(hdr.plen() >= KEYSIZE + _offset);

    SHA1_init(&sha_ctx);
    SHA1_update(&sha_ctx, (unsigned char *)&hdr.hdr()->last, hdr_size - offsetof(struct click_xia, last));
    SHA1_final(digest, &sha_ctx);

    memcpy(hdr.payload() + _offset, digest, KEYSIZE);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAInsertHash)
