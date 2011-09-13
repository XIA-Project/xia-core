#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/vector.hh>

#include <click/xiacontentheader.hh>
#include "xiatransport.hh"
#include "xudp.hh"

//TODO: Add content support

CLICK_DECLS
XUDP::XUDP()
{
    _id=0;
}

	int
XUDP::configure(Vector<String> &conf, ErrorHandler *errh)
{
	XIAPath local_addr;
	if (cp_va_kparse(conf, this, errh,
				"LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
				"CLICK_IP", cpkP+cpkM, cpIPAddress, &_CLICKaddr,
				"API_IP", cpkP+cpkM, cpIPAddress, &_APIaddr,
				cpEnd) < 0)
		return -1;

	_local_addr = local_addr;
	_local_hid = local_addr.xid(local_addr.destination_node());
	return 0;
}

XUDP::~XUDP()
{

	//Clear all hashtable entries
	XIDtoPort.clear();
	portToDAGinfo.clear();
	portRxSeqNo.clear();
	portTxSeqNo.clear();

}



void XUDP::push(int port, Packet *p_in)
{
	WritablePacket *p = p_in->uniqueify();
	//Depending on which port it arrives at it could be control/API traffic/Data traffic
	switch(port){
		case 0: //Control traffic
			{
				//Extract the destination port 
				click_udp * uh=p->udp_header();
				click_ip * ih=p->ip_header();
				unsigned short _dport=uh->uh_dport;
				unsigned short _sport=uh->uh_sport;

				//Depending on destination port it could be open/close/bind call
				switch(_dport){
					case CLICKOPENPORT: //Open socket. Reply with a packet with the destination port=source port
						{
							portRxSeqNo.set(_sport,1);
							uh->uh_dport=_sport;
							struct in_addr temp;
							temp=ih->ip_src;
							ih->ip_src=ih->ip_dst;
							ih->ip_dst=temp;
							portTxSeqNo.set(_sport,1);
							output(0).push(p);
						}
						break;

					case CLICKBINDPORT: //Bind XID
						{
							String xid_string((const char*)p->data(),(int)p->length());
							XID xid(xid_string);

							XIDtoPort.set(xid,_sport);//TODO: Maybe change the mapping to XID->DAGinfo?
							
							//Set source DAG info
							DAGinfo daginfo;
							daginfo.port=_sport;
							String str_local_addr=_local_addr.unparse_re();
							str_local_addr=str_local_addr+" "+xid_string;//Make source DAG _local_addr:SID
							daginfo.src_path.parse_re(str_local_addr);//TODO: Check
							daginfo.nxt=-1;
							daginfo.last=-1;
							daginfo.hlim=250;

							portToDAGinfo.set(_sport,daginfo);


							portRxSeqNo.set(_sport,portRxSeqNo.get(_sport)+1);//Increment counter
						}
						break;

					case CLICKCLOSEPORT: //Close port
						{
							XIDtoPort.erase(portToDAGinfo.get(_sport).xid);
							portToDAGinfo.erase(_sport);
							portRxSeqNo.erase(_sport);                
							portTxSeqNo.erase(_sport);

						}
						break;

					case CLICKCONNECTPORT://Connect
						{
							String dest((const char*)p->data(),(int)p->length());
							
							XIAPath dst_path;
							dst_path.parse(dest); //TODO: Check 

                            DAGinfo daginfo=portToDAGinfo.get(_sport);

							daginfo.dst_path=dst_path;

							portToDAGinfo.set(_sport,daginfo);
    						portRxSeqNo.set(_sport,portRxSeqNo.get(_sport)+1);//Increment counter
						}					
					
				}
				//TODO: Send a confirmation to API for control messages
			}
			break;


		case 1: //packet from Socket API
			{
				//Extract the source port 
				const click_udp * udp_header=p->udp_header();

				unsigned short _sport=udp_header->uh_sport;
				
                //Find DAG info for that stream
				DAGinfo daginfo=portToDAGinfo.get(_sport);
				
				//Add XIA headers
				class XIAHeaderEncap* _xiah=new XIAHeaderEncap();
				if (daginfo.nxt >= 0)
					_xiah->set_nxt(-1);
				_xiah->set_last(-1);
				_xiah->set_hlim(250);
				_xiah->set_dst_path(daginfo.dst_path);
				_xiah->set_src_path(daginfo.src_path);
				
				p->pull(p->transport_header_offset());//Remove IP header
				p_in->pull(8); //Remove UDP header
				
				//Might need to remove more if another header is required (eg some control/DAG info)
				
				WritablePacket *p = NULL;
                p = _xiah->encap(p_in, true);

				output(1).push(p);
			}

			break;

		case 2://Packet from network layer
			{
				//Extract the SID/CID 
				XIAHeader x_hdr(p->xia_header());
			    XIAPath dst_path=x_hdr.dst_path();
                XID	_destination_xid = dst_path.xid(dst_path.destination_node());          

				unsigned short _dport= XIDtoPort.get(_destination_xid);
				
                //TODO: Refine the way we change DAG in case of migration. Use some control bits. Add verification
   				DAGinfo daginfo=portToDAGinfo.get(_dport);
   				daginfo.dst_path=x_hdr.src_path();
   				portToDAGinfo.set(_dport,daginfo);
                			
				output(2).push(UDPIPEncap(p,_dport));
			}

			break;

	}

}

Packet *
XUDP::UDPIPEncap(Packet *p_in, int dport)
{
	WritablePacket *p = p_in->push(sizeof(click_udp) + sizeof(click_ip));
	click_ip *ip = reinterpret_cast<click_ip *>(p->data());
	click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

#if !HAVE_INDIFFERENT_ALIGNMENT
	assert((uintptr_t)ip % 4 == 0);
#endif
	// set up IP header
	ip->ip_v = 4;
	ip->ip_hl = sizeof(click_ip) >> 2;
	ip->ip_len = htons(p->length());
	ip->ip_id = htons(_id.fetch_and_add(1));
	ip->ip_p = IP_PROTO_UDP;
	ip->ip_src = _CLICKaddr;
	ip->ip_dst = _APIaddr;
	p->set_dst_ip_anno(IPAddress(_APIaddr));

	ip->ip_tos = 0;
	ip->ip_off = 0;
	ip->ip_ttl = 250;
	_cksum=false;

	ip->ip_sum = 0;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
	if (_aligned)
		ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
	else
		ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
	ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
	ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif

	p->set_ip_header(ip, sizeof(click_ip));

	// set up UDP header
	udp->uh_sport = dport;
	udp->uh_dport = dport;

	uint16_t len = p->length() - sizeof(click_ip);
	udp->uh_ulen = htons(len);
	udp->uh_sum = 0;
	if (_cksum) {
		unsigned csum = click_in_cksum((unsigned char *)udp, len);
		udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, len);
	}

	return p;
}


	CLICK_ENDDECLS
EXPORT_ELEMENT(XUDP)
	//ELEMENT_REQUIRES(userlevel)
	ELEMENT_REQUIRES(XIAContentModule)
ELEMENT_MT_SAFE(XUDP)
