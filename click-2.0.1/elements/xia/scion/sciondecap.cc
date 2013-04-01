#include <click/config.h>
#include "sciondecap.hh"
#include "packetheader.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <arpa/inet.h>

CLICK_DECLS

SCIONDecap::SCIONDecap():_path_info(0)
{
}

SCIONDecap::~SCIONDecap()
{
}

int
SCIONDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String table_file;
    if (Args(conf, this, errh)
    .read("PATHINFO", ElementCastArg("SCIONPathInfo"), _path_info)
    .complete() < 0)
        return -1;

    return 0;
}

int
SCIONDecap::initialize(ErrorHandler *)
{
    return 0;
}

/* Remove the SCION header to forward the packet to the IPv4 network
SL: simple_action needs to be changed to push or pull since it's slow...
*/
Packet *
SCIONDecap::simple_action(Packet *p)
{
    if (!p)
        return 0;

    //unsigned const char *data = p->data();
    uint8_t *data = (uint8_t *)p->data();
    
    uint8_t type = SCIONPacketHeader::getType(data);

    if (type == DATA) {
		//SL: getHdrLen should return the SCION header length including opaque fields.
        int payload_offset = SCIONPacketHeader::getHdrLen(data);
		
		//1. if _path_info is set, caches OF for the return packet
		if(_path_info) {
        	/* when a reverse path is cached by IP address of the sender*/
			const struct click_ip *ip_header = (const struct click_ip *)(data+payload_offset);
        	assert(ip_header->ip_v == 4);

        	struct in_addr src_ip;
        	memcpy(&src_ip, &(ip_header->ip_src), sizeof(struct in_addr));
			//////////////////////////////////////////////////////////////

			uint8_t srcLen = SCIONPacketHeader::getSrcLen(data);
			uint8_t dstLen = SCIONPacketHeader::getDstLen(data);
			uint8_t of_offset = COMMON_HEADER_SIZE+srcLen+dstLen;
			time_t t;
			time(&t);
		
			fullPath path;
			path.length = payload_offset - of_offset;
			//SLT: this allocation should be freed when the path is deleted...
			//only sciondecap does not free this memory (though it causes very small mem. leak).
			//this should be modified after caching policy
			path.opaque_field = new uint8_t [path.length];
			memcpy(path.opaque_field, data+of_offset, path.length);
			path.cache_time = t;
			
			//a) when a reverse path is cached by Gateway address of the origin
			//HostAddr srcAddr = SCIONPacketHeader::getSrcAddr(data);
			//b) when a reverse path is cached by IP address of the sender
			HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, (uint64_t) src_ip.s_addr);
			
			_path_info->storeOpaqueField(srcAddr, path);
			#ifdef _SL_DEBUG_GW
			printf("Return Path is stored:length = %d\n",path.length);
			#endif
		}
		
		//2. then remove the SCION header and forward
		//SL: remove the payload by the offset from the beginning.
		//Click specific...
        p->pull(payload_offset);

        return p;

    } else {
		//SL: this part should be also added to the logfile.
        click_chatter("unexpected packet type %d", type);
        return 0;
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONDecap)
