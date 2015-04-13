/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
** 
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
** 
** http://www.apache.org/licenses/LICENSE-2.0 
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include <click/config.h>
#include "scionencap.hh"
#include "packetheader.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <arpa/inet.h>
#include <fstream>
#include <string>
#include "config.hh"

#include <click/xiaheader.hh>
#include <click/xiacontentheader.hh>
#include <click/xiatransportheader.hh>
#include <click/xid.hh>
#include <click/standard/xiaxidinfo.hh>
#include "xiatransport.hh"
#include "xiaxidroutetable.hh"
#include "xtransport.hh"

#define SID_XROUTE  "SID:1110000000000000000000000000000000001112"

CLICK_DECLS

int SCIONEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if(cp_va_kparse(conf, this, errh, 
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile,
       cpEnd) <0){
           click_chatter("atal error: click configuration fail at SCIONEncap.\n");
            exit(-1);
    }

    XIAXIDInfo xiaxidinfo;
    struct click_xia_xid store;
    XID xid = xid;

    xiaxidinfo.query_xid(m_AD, &store, this);
    xid = store;
    m_AD = xid.unparse();

    xiaxidinfo.query_xid(m_HID, &store, this);
    xid = store;
    m_HID = xid.unparse();

    return 0;
}

int SCIONEncap::initialize(ErrorHandler *)
{

    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
	// Encap only needs to know PS location
    parser.parseServers(m_servers);

    if (!_path_info){
		#ifdef _DEBUG_GW
		click_chatter("A new path info is being created...\n");
		#endif
        _path_info = new SCIONPathInfo;
	}

	_timer.initialize(this);
	_timer.schedule_after_sec(5);

    return 0;
}

void SCIONEncap::push(int port, Packet *p) {

    if(port == 0)
    {
        // from XIA network
        TransportHeader thdr(p);
        uint8_t * s_pkt = (uint8_t *) thdr.payload();
        uint16_t totalLength = SPH::getTotalLen(s_pkt);
        uint8_t packet[totalLength];
        memcpy(packet, s_pkt,totalLength);
        uint16_t type = SPH::getType(s_pkt);
        p->kill();
        
        switch(type)
        {
            case UP_PATH: {
                #ifdef _DEBUG_GW
                click_chatter("GW (%lu:%lu): uppath received.\n", m_uAdAid, m_uAid);
                #endif
                _path_info->parse(packet, 0);
            }
                break;
            
            case PATH_REP_LOCAL:{
                #ifdef _DEBUG_GW
			    click_chatter("GW (%lu:%lu): downpath received.\n", m_uAdAid, m_uAid);
			    #endif
			    _path_info->parse(packet, 1);
            }
                break;
            
            default:
                break;
        }
        
        fullPath path;
		path.opaque_field = NULL;
		uint64_t src = 4;
		uint64_t dst = 3;
		
		int skip = 1;
        if(_path_info->get_path(src, dst, path)) {
            printf("Full path information:\n");
            #ifdef _DEBUG_GW
            for(int j = 0; j<(path.length / OPAQUE_FIELD_SIZE); j++){
                opaqueField *of = (opaqueField *)(path.opaque_field+j*OPAQUE_FIELD_SIZE);
                if(of->type==0x00&&skip) {
                    printf("%u ->", of->ingressIf);
                }
                    
                if(of->type==0x20){
                    printf("%u ->", of->egressIf);
                    if(skip==0) skip=1;
                    else if(skip==1) skip=0;
                }
			}
			printf("\n");
			#endif
        }
        
        if(path.opaque_field)
		    delete [] path.opaque_field;
        /*
		// check if there's any packet in the cache 
		// and send it to the destination after constructing end-to-end paths.
        for (std::list<PacketCache>::iterator it = _cache.begin(); it != _cache.end(); ++it)
        {
            fullPath path;
			path.opaque_field = NULL;

            if (_path_info->get_path((ADAID)m_uAdAid, it->dst_gw.adaid, path)) {
                // send this packet
                #ifdef _DEBUG_GW
                click_chatter("sending a saved packet from %ld to %ld.", m_uAdAid, it->dst_gw.adaid);
                #endif
				//handling already cached packets, so don't need to cache it again
                handle_data(it->p, it->dst_gw, false);
                it = _cache.erase(it);
                if (it != _cache.begin())
                    --it;
            } 
				
			if(path.opaque_field)
				delete [] path.opaque_field;
        }
        */
    }else{
        // from client socket
        
    }

}

void SCIONEncap::run_timer(Timer* timer)
{

    sendHello();

    if(m_servers.find(PathServer)!=m_servers.end()){
        
        //remove paths to get fresh ones...
        //_path_info->clearUpPaths();
        //_path_info->clearDownPaths();
        m_bClearReversePath = true; //clear paths

        // retrieve a path, send a path request
        uint16_t length = COMMON_HEADER_SIZE + AIP_SIZE*2 + PATH_INFO_SIZE;
        uint8_t packet[length];
        memset(packet, 0, length);

        scionHeader hdr;
        hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1));
        hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)(m_servers.find(PathServer)->second.HID));
        hdr.cmn.type = PATH_REQ_LOCAL;
        hdr.cmn.hdrLen = COMMON_HEADER_SIZE + AIP_SIZE*2;
        hdr.cmn.totalLen = length;
        SPH::setHeader(packet,hdr);

        pathInfo* pathInformation = (pathInfo*)(packet + COMMON_HEADER_SIZE + AIP_SIZE*2);
        pathInformation->target = 3; 
        // TODO currently hardcoded to AD3 to test path resolution
        pathInformation->tdid = 1;
        pathInformation->option = 0;
        
        string dest = "RE ";
        dest.append(BHID);
        dest.append(" ");
        dest.append(m_AD.c_str());
        dest.append(" ");
        dest.append("HID:");
        dest.append((const char*)m_servers.find(PathServer)->second.HID);

        sendPacket(packet, length, dest);
    
    }else{
        #ifdef _DEBUG_GW
		click_chatter((char*)"AD (%s) does not has path server.\n", m_AD.c_str());
		#endif
    }
    
	_timer.reschedule_after_sec(5);
}

#if 0
void SCIONEncap::handle_data(Packet *p_in, GatewayAddr &dst_gw, bool store)
{
    if (!p_in)
        return;

    // retrieve a path
    fullPath path;
	//make sure that the opaque fields are empty...
	path.opaque_field = NULL;
	HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
	//SL: This should be somehow modified later to deal with different address types
	uint8_t srcLen = srcAddr.getLength();
	uint8_t dstLen = SCION_ADDR_SIZE;
	//1. Cannot find the path to the dst IP
	//1.1 keep the current packet in the cache
	//1.2 send a query to PS for dst
    if (!_path_info->get_path((ADAID)m_uAdAid, dst_gw.adaid, path) || m_bClearReversePath) {

        // can't find a path, store the packet and send a request
        click_chatter("can't find a path from %lld to %lld. packet saved.", m_uAdAid, dst_gw.adaid);

        // store the packet
		//SLP: store the packet with dst as a key
		//need to delete the memory for the packet cache later
        if (store) {
            PacketCache packet_store;
            packet_store.p = p_in;
            packet_store.dst_gw.adaid = dst_gw.adaid;
            packet_store.dst_gw.addr = dst_gw.addr;
            _cache.push_back(packet_store);
			//SLT: MAX_CACHE_SIZE needs to be set
			if(_cache.size() > 100){
				(*_cache.begin()).p->kill();
				_cache.pop_front();
			}
        }

        // send a path request
        uint16_t length = COMMON_HEADER_SIZE + srcLen + dstLen + PATH_INFO_SIZE;
        uint8_t packet[length];
        memset(packet, 0, length);

		//SL: set the path server address here
		//include the PS address in the configuration file
		//only PS address is necessary for clients
		//PS address should be read from the configuration file
		//e.g., the same configuration file as the gateway and decap
		scionHeader hdr;

		HostAddr psAddr = m_servers.find(PathServer)->second.addr;
		hdr.src = srcAddr;
		hdr.dst = psAddr; //m_servers.find(PathServer)->second.addr;
		
		hdr.cmn.type = PATH_REQ_LOCAL;
		hdr.cmn.hdrLen = COMMON_HEADER_SIZE + hdr.src.getLength() + hdr.dst.getLength();
		hdr.cmn.totalLen = length;

		SPH::setHeader(packet,hdr);

		#ifdef _SL_DEBUG_GW
		printf("\tSrc: %lu (Len:%d, Type:%d), Dst: %lu (Len:%d, Type:%d), tdid=%d, adaid = %lu\n", 
			srcAddr.numAddr(), srcAddr.getLength(), srcAddr.getType(),
			psAddr.numAddr(), psAddr.getLength(), psAddr.getType(), dst_gw.tdid, dst_gw.adaid);
		#endif

        pathInfo* pathInformation = (pathInfo*)(packet + COMMON_HEADER_SIZE + srcLen + dstLen);
		//SL: need to check if these information is correct
		///////////////////////////////////////////////////
        pathInformation->target = dst_gw.adaid;
        pathInformation->tdid = dst_gw.tdid; //SL: this needs to be revised
        pathInformation->option = 0;
		///////////////////////////////////////////////////
        WritablePacket* out_packet = Packet::make(DEFAULT_HD_ROOM, packet, length, DEFAULT_TL_ROOM);
        output(0).push(out_packet);

		//SLP: memory path.opaque_field must be freed 
		//Yet, it's better to define fullPath as a class and do this in the destructor
		if(path.opaque_field)
			delete [] path.opaque_field;

		//why removing path info here???
		//SLT: temporarily blocked since I am not sure why they are removed...
        _path_info->clearUpPaths();
        _path_info->clearDownPaths();
	//2. path to the dst AD exists
    } else {
    
        click_chatter("found a path from %lld to %lld. send the packet.", m_uAdAid, dst_gw.adaid);
        // found a path
		//SL: Host addresses need to be changed to those of gateways? or destination IPs?...
		uint8_t srcLen = srcAddr.getLength();
		uint8_t dstLen = dst_gw.addr.getLength();
		uint8_t offset = srcLen + dstLen;
		uint8_t hdrLen = path.length + COMMON_HEADER_SIZE + offset;
        
        Packet *p = p_in->push(hdrLen);
        uint8_t *header = (uint8_t *)p->data();
		
		scionHeader hdr;

		hdr.src = srcAddr;
		hdr.dst = dst_gw.addr; 
		
		hdr.cmn.type = DATA;
		hdr.cmn.hdrLen = hdrLen;
		hdr.cmn.totalLen = p->length();
		hdr.cmn.currOF = offset;
		hdr.cmn.timestamp = offset;
		hdr.cmn.flag |= UP_PATH_FLAG; //set uppath flag; alternatively use SPH::setUppathFlag(header)

		hdr.n_of = path.length / OPAQUE_FIELD_SIZE;
		hdr.p_of = path.opaque_field;

		SPH::setHeader(header,hdr);

		#ifdef _SL_DEBUG
		for(int j = 0; j<hdr.n_of; j++){
			printf("Snd: AD(%llu): %d th OF: %02x\n",
				m_uAdAid, j+1,*(uint8_t *)(path.opaque_field+j*OPAQUE_FIELD_SIZE));
		}
		#endif
		
		//SLP: memory path.opaque_field must be freed here
		//Yet, it's better to define fullPath as a class and do this in the destructor
		delete [] path.opaque_field;
        output(0).push(p);
    }
}

void
SCIONEncap::handle_data(Packet *p_in, GatewayAddr &dst_gw, fullPath &path, bool store)
{
    if (!p_in)
        return;

    // retrieve a path
	HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
        
	uint8_t srcLen = srcAddr.getLength();
	uint8_t dstLen = dst_gw.addr.getLength();
	uint8_t offset = srcLen + dstLen;
	uint8_t hdrLen = path.length + COMMON_HEADER_SIZE + offset;
	
	Packet *p = p_in->push(hdrLen); 
	uint8_t *header = (uint8_t *)p->data();

	scionHeader hdr;

	hdr.src = srcAddr;
	hdr.dst = dst_gw.addr; 
	
	hdr.cmn.type = DATA;
	hdr.cmn.hdrLen = hdrLen;
	hdr.cmn.totalLen = p->length();
	hdr.cmn.currOF = offset;
	hdr.cmn.timestamp = offset;
	hdr.cmn.flag |= 0x80; //set uppath flag; alternatively use SPH::setUppathFlag(header)

	hdr.n_of = path.length / OPAQUE_FIELD_SIZE;
	hdr.p_of = path.opaque_field;

	SPH::setHeader(header,hdr);
	
	output(0).push(p);
}
#endif

void SCIONEncap::sendPacket(uint8_t* data, uint16_t data_length, string dest) {

    string src = "RE ";
    src.append(m_AD.c_str());
    src.append(" ");
    src.append(m_HID.c_str());
    src.append(" ");
    src.append(SID_XROUTE);

	XIAPath src_path, dst_path;
	src_path.parse(src.c_str());
	dst_path.parse(dest.c_str());

    XIAHeaderEncap xiah;
    xiah.set_nxt(CLICK_XIA_NXT_TRN);
    xiah.set_last(LAST_NODE_DEFAULT);
    //xiah.set_hlim(hlim.get(_sport));
    xiah.set_src_path(src_path);
    xiah.set_dst_path(dst_path);

    WritablePacket *p = Packet::make(DEFAULT_HD_ROOM, data, data_length, DEFAULT_TL_ROOM);
    TransportHeaderEncap *thdr = TransportHeaderEncap::MakeDGRAMHeader(0); // length
	WritablePacket *q = thdr->encap(p);

    thdr->update();
    xiah.set_plen(data_length + thdr->hlen()); 
    // XIA payload = transport header + transport-layer data

    q = xiah.encap(q, false);
	output(0).push(q);
}

void SCIONEncap::sendHello() {
    string msg = "0^";
    msg.append(m_AD.c_str());
    msg.append("^");
    msg.append(m_HID.c_str());
    msg.append("^");
    string dest = "RE ";
    dest.append(BHID);
    dest.append(" ");
    dest.append(SID_XROUTE);
    sendPacket((uint8_t *)msg.c_str(), msg.size(), dest);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONEncap)
ELEMENT_MT_SAFE(SCIONEncap)
