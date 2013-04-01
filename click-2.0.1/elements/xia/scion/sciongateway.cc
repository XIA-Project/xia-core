#include <click/config.h>
#include "sciongateway.hh"
#include "packetheader.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <arpa/inet.h>
#include <fstream>
CLICK_DECLS

SCIONGateway::SCIONGateway() :
m_uAid(0)
{
}

SCIONGateway::~SCIONGateway()
{
}

int
SCIONGateway::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String table_file;
    if (Args(conf, this, errh)
    .read_mp("AID", m_uAid)
    .complete() < 0)
        return -1;

    return 0;
}

int
SCIONGateway::initialize(ErrorHandler *)
{
    return 0;
}


    // input
    //   0: from SCION Switch (AID Request / Data / Path)
    //   1: from SCION Encap  (Data / Path Request)
    // output
    //   0: to SCION switch (Data / Path Request / AID Reply)
    //   1: to SCION Encap (Path)
    //   2: to SCION Decap (Data)
    //
    // packet flow
    //   input 0: 
    //     aid req -> (locallly handled) -> output 0
    //     data -> output 2
    //     path -> output 1
    //   input 1:
    //     data/path req -> output 0 



void
SCIONGateway::push(int port, Packet *p)
{
    // all packets are SCION..
    unsigned const char *data = p->data();
    uint16_t type = SCIONPacketHeader::getType((uint8_t *)data);

	//1. from Switch
    if (port == 0) {

        if (type == DATA) {
            output(2).push(p);

        } else if (type == AID_REQ) {
            WritablePacket *q = p->uniqueify();
            unsigned char *data_out = q->data();
            uint64_t *ptr = (uint64_t*)(data_out + COMMON_HEADER_SIZE);
            *ptr = m_uAid;
            
            SCIONPacketHeader::setType((uint8_t *)data_out, AID_REP);
			HostAddr addr(HOST_ADDR_SCION,m_uAid);
       		SCIONPacketHeader::setSrcAddr((uint8_t *)data_out, addr);
            
            output(0).push(q);

        } else if (type == UP_PATH || type == PATH_REP_LOCAL) {
			#ifdef _SL_DEBUG_GW
			printf("Path reply from PS: Addr: %llu\n", m_uAid);
			#endif
            output(1).push(p);
        } else {
            p->kill();
            click_chatter("unexpected packet type.");
        }
	//2. from encap element
    } else {

        if (type == DATA || type == PATH_REQ_LOCAL) {
			#ifdef _SL_DEBUG_GW
			if(type == PATH_REQ_LOCAL) {
				HostAddr srcAddr = SCIONPacketHeader::getSrcAddr((uint8_t *)p->data());
				HostAddr dstAddr = SCIONPacketHeader::getDstAddr((uint8_t *)p->data());
				printf("Path request to PS: GW Addr:%llu, srcAddr: %llu, dstAddr: %llu\n", 
					m_uAid, srcAddr.numAddr(), dstAddr.numAddr());
			}
			#endif
			//SLA:
			//IPv4 handling here
			//if addrtype == IPv4, encap with IP header
			//otherwise
            output(0).push(p);
        } else {
            click_chatter("unexpected packet type %d", type);
        }
    }

}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONGateway)
ELEMENT_MT_SAFE(SCIONGateway)
