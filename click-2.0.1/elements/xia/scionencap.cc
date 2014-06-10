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

SCIONEncap::SCIONEncap() :
m_uAdAid(0), _path_info(0),m_bDefaultDestOn(false),m_bClearReversePath(false),_timer(this)
{
}

//SL: read configuration file
//how to read/store SCIONPathInfo???
int
SCIONEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if(cp_va_kparse(conf, this, errh, 
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
       cpEnd) <0){

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

void SCIONEncap::run_timer(Timer* timer)
{
    sendHello();

	//remove paths to get fresh ones...
	//this is just for a test; needs to be elaborated...
    //_path_info->clearUpPaths();
    //_path_info->clearDownPaths();
	m_bClearReversePath = true; //clear paths

    // retrieve a path
    fullPath path;
	//make sure that the opaque fields are empty...
	path.opaque_field = NULL;
    uint64_t src_HID = atoi(m_HID.c_str()+4);
	HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, src_HID);
	//SL: This should be somehow modified later to deal with different address types
	uint8_t srcLen = srcAddr.getLength();
	uint8_t dstLen = SCION_ADDR_SIZE;

	//1.2 send a query to PS for dst

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

    pathInfo* pathInformation = (pathInfo*)(packet + COMMON_HEADER_SIZE + srcLen + dstLen);
    //SL: need to check if these information is correct
    ///////////////////////////////////////////////////
    pathInformation->target = 3; // TODO currently hardcoded to AD3 to test path resolution
    pathInformation->tdid = 0;
    pathInformation->option = 0;
    ///////////////////////////////////////////////////

    //printf("\tSrc: %lu (Len:%d, Type:%d), Dst: %lu (Len:%d, Type:%d), tdid=%d, adaid = %lu\n", 
        //srcAddr.numAddr(), srcAddr.getLength(), srcAddr.getType(),
        //psAddr.numAddr(), psAddr.getLength(), psAddr.getType(), 12345, 6789);

    string dest = "RE ";
    dest.append(BHID);
    // TODO: hardcoded local path server
    dest.append(" ");
    dest.append(m_AD.c_str());
    dest.append(" ");
    dest.append("HID:");
    dest.append((const char*)"0000000000000000000000000000000000100000");

    sendPacket(packet, length, dest);

    //SLP: memory path.opaque_field must be freed 
    //Yet, it's better to define fullPath as a class and do this in the destructor
    if(path.opaque_field)
        delete [] path.opaque_field;

	_timer.reschedule_after_sec(5);
}

int
SCIONEncap::initialize(ErrorHandler *)
{

    //initVariables(); 
	//SLT: Currently has an error -> need to set logfile
    //scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile);

    if (!_path_info){
		#ifdef _SL_DEBUG_GW
		printf("A new path info is being created...\n");
		#endif
        _path_info = new SCIONPathInfo;
	}

#if 0
    // load ip to ad indentifier table
	//SL: this table contains a map of IP addresses to the corresponding ADs
	//ifstream might be slow yet okay since this is one time operation.
    std::ifstream table(m_sAddrTableFile.c_str());
    while (!table.eof()) {
        char str_ip[16]; //IP address in char array
		std::string string_ip; //IP address in string
		
		GatewayAddr gw; //Gateway's AD and ADDR
		uint64_t gw_addr;
        struct in_addr binary_ip; //SL: in_addr is unsigned long type.
		if(table.peek() == '#') {
			char str[256];
			table.getline(str, 256);
			
			#ifdef _SL_DEBUG
			printf("GW: Found comment line -> would skip!!!!\n");
			printf("Line to skip: %s\n", str);
			#endif
			
			continue;
		}
        //table >> str_ip >> gw.tdid >> gw.adaid >> gw_addr;
        table >> string_ip >> gw.tdid >> gw.adaid >> gw_addr;
		gw.addr.setSCIONAddr(gw_addr);

		//SL: if address include '/' (i.e., prefix), add it to the corresponding table
		//currently, we support /24 network address (i.e., netmask == 24), but would change this
		//to support all prefixes.
		size_t pos = string_ip.find('/');
		if(pos!=std::string::npos) {
			int prefix_len = atoi(string_ip.substr(pos+1).c_str());
			if(!prefix_len) {// default destination for all packets
				m_bDefaultDestOn = true;
				_gw_default = gw;
			} else {
	        	inet_pton(AF_INET, string_ip.c_str(), &binary_ip);
				binary_ip.s_addr = binary_ip.s_addr >> 8; //this should be changed to >> 32-prefix_len
				_ad_table_24[binary_ip.s_addr]=gw;
			}
		} else { //a single IP
	
			//SL: The address type of a gateway is 64bit SCION Addr
			//This should be extended to any address type later
	        inet_pton(AF_INET, string_ip.c_str(), &binary_ip);
	        _ad_table[binary_ip.s_addr] = gw;
		}
		#ifdef _SL_DEBUG_GW
		printf("GW: str_ip: %s, gw.tdid: %d, gw.adaid: %llu, gw_addr:%llu\n",
			string_ip.c_str(), gw.tdid, gw.adaid, gw_addr);
		#endif
		
    }
#endif

	_timer.initialize(this);
	_timer.schedule_after_sec(5);

    return 0;
}

/*
    SCIONEncap::parseTopology
    - parses topology file using topo parser
*/
#if 0
void SCIONEncap::parseTopology(){
    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
	//SL: Encap only needs to know PS location
    parser.parseServers(m_servers);
	//SL: Encap has to map ifid to Addr to send data packet 
    parser.parseRouters(m_routers);
}


/* SLT:
	SCIONEncap::constructIfid2AddrMap() {
	Construct ifid2addr map 
*/
void
SCIONEncap::constructIfid2AddrMap() {
	std::multimap<int, RouterElem>::iterator itr;
	for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
		ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interface.id, itr->second.addr));
	}
}

/*
    SCIONEncap::initVariables
    Initialize from config and topology files
*/
void SCIONEncap::initVariables(){
    Config config;
    config.parseConfigFile((char*)m_sConfigFile.c_str());
    config.getLogFilename(m_csLogFile);
    m_uAdAid = config.getAdAid();
    m_uTdAid = config.getTdAid();
    m_iLogLevel = config.getLogLevel();
    
    parseTopology();
	constructIfid2AddrMap();
}
#endif

/*Handle incoming IP data
SL: dst_adaid (not dst address)
*/
void
SCIONEncap::handle_data(Packet *p_in, GatewayAddr &dst_gw, bool store)
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

/*Handle incoming IP data
SL: dst_adaid (not dst address)
*/
void
SCIONEncap::handle_data(Packet *p_in, GatewayAddr &dst_gw, fullPath &path, bool store)
{
    if (!p_in)
        return;

    // retrieve a path
	HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
        
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
	hdr.cmn.flag |= 0x80; //set uppath flag; alternatively use SPH::setUppathFlag(header)

	hdr.n_of = path.length / OPAQUE_FIELD_SIZE;
	hdr.p_of = path.opaque_field;

	SPH::setHeader(header,hdr);
	
	output(0).push(p);
}


// input 0: IP networks
// input 1: SCION networks (SCION switch)
// output 0: SCION switch

void
SCIONEncap::push(int port, Packet *p)
{
#if 0
    unsigned const char *data = p->data();

	//1. input from an IP client (or IP network)
	//SL: should we support multiple IP nodes?
	//in such case, port 0 is better to be assigned to the SCION network (to the gateway)
    if (port == 0) { // data, must be ip datagram
        const struct click_ip *ip_header = (const struct click_ip *)data;
        assert(ip_header->ip_v == 4);

        struct in_addr dest_ip;
        memcpy(&dest_ip, &(ip_header->ip_dst), sizeof(struct in_addr));

		//find the AD where dest_ip belongs.
		//1.1 dest_ip exists in the ad table
		//SL: two tables need to be looked up; i.e., _ad_table, _ad_table_24
		//this should be integrated together (e.g., like IP forwarding table)
		GatewayAddr gw_addr;
		bool hasEntry = true;
		if(m_bDefaultDestOn) {
			gw_addr = _gw_default;
		} else {
			if(_ad_table.find(dest_ip.s_addr) != _ad_table.end()){
				gw_addr = _ad_table[dest_ip.s_addr];

			} else if(_ad_table_24.find(dest_ip.s_addr >> 8) != _ad_table_24.end()) {
				gw_addr = _ad_table[dest_ip.s_addr >> 8];
			} else {
				hasEntry = false;
			}
		}
        if (hasEntry) {
			//1.1.1 return path already exist
			HostAddr dstAddr = HostAddr(HOST_ADDR_IPV4, dest_ip.s_addr);
			if(_path_info->m_inOF.find(dstAddr) != _path_info->m_inOF.end() && !m_bClearReversePath) {
				#ifdef _SL_DEBUG_GW
				printf("Sending packet using a reverse path\n");
				#endif
				/*a) when a reversed path is cached by Gateway Addr.*/
				//list<fullPath>::reverse_iterator rItr = _path_info->m_inOF[gAddr.addr].rbegin();
				/*b) when a reverse path is cached by IP address*/
				_path_info->_lock.acquire_read();
				list<fullPath>::reverse_iterator rItr = _path_info->m_inOF[dstAddr].rbegin();
            	//handle_data(p, _ad_table[dest_ip.s_addr], *rItr, true);
            	handle_data(p, gw_addr, *rItr, true);
				_path_info->_lock.release_read();
			//1.1.2 path to the destination needs to be constructed
			} else {
            	//handle_data(p, _ad_table[dest_ip.s_addr], true);
				#ifdef _SL_DEBUG
				printf("Create a new path!!!\n");
				#endif
            	handle_data(p, gw_addr, true);
				m_bClearReversePath = false;
			}
        //1.2 dest_ip doesn't exist in the ad table
		} else {
			char straddr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET,&dest_ip,straddr,INET_ADDRSTRLEN);
            click_chatter("ip mapping entry for (%s) not found!",straddr);
        }
    //2. input from the connected SCION gateway (for path reply)    
    } else {
    	//copy the data of the packet and kills click packet
		int type = SPH::getType((uint8_t*)p->data());
		uint16_t packetLength = SPH::getTotalLen((uint8_t*)p->data());
		uint8_t packet[packetLength];
		memset(packet, 0, packetLength);
		memcpy(packet, (uint8_t*)p->data(), packetLength);
		p->kill();
#endif

    TransportHeader thdr(p);
    uint8_t * s_pkt = (uint8_t *) thdr.payload();
    uint16_t totalLength = SPH::getTotalLen(s_pkt);
    uint8_t packet[totalLength];
    memcpy(packet, s_pkt,totalLength);
    uint16_t type = SPH::getType(s_pkt);
    p->kill();

        if (type == UP_PATH || type == PATH_REP_LOCAL) {
            //click_chatter("path received.");
			//SL: 
			//1. parse uppath and store it in the path_info
            if (type == UP_PATH) {
				//#ifdef _SL_DEBUG_GW
                //printf("Encap (%lu:%lu): uppath received\n", m_uAdAid, m_uAid);
				//#endif
                _path_info->parse(packet, 0);
			//2. parse downpath and store it in the path_info
            } else {
				//#ifdef _SL_DEBUG_GW
                //printf("Encap (%lu:%lu): downpath received\n", m_uAdAid, m_uAid);
				//#endif
                _path_info->parse(packet, 1);
            }
            
			//SL: check if there's any packet in the cache and send it to the destination
			//after constructing end-to-end paths.
			//better to do this for the cached packets that belong to the received downpath
            for (std::list<PacketCache>::iterator it = _cache.begin(); it != _cache.end(); ++it)
            {
                fullPath path;
				path.opaque_field = NULL;

                if (_path_info->get_path((ADAID)m_uAdAid, it->dst_gw.adaid, path)) {
                    // send this packet
                    click_chatter("sending a saved packet from %ld to %ld.", m_uAdAid, it->dst_gw.adaid);
					//handling already cached packets, so don't need to cache it again
                    handle_data(it->p, it->dst_gw, false);
                    it = _cache.erase(it);
                    if (it != _cache.begin())
                        --it;
                } 
				
				if(path.opaque_field)
					delete [] path.opaque_field;
            }
        //SL: this should not happen??
		//why a data packet is sent to encap?
        } 
#if 0
        else if (type == DATA) {
            click_chatter("data received.");
        } else {
            click_chatter("unexpected packet type %d", type);
        }
    }
#endif
}

void SCIONEncap::sendPacket(uint8_t* data, uint16_t data_length, string dest) {
    string src = "RE ";
    src.append(m_AD.c_str());
    src.append(" ");
    src.append(m_HID.c_str());
    src.append(" ");
    src.append(SID_XROUTE);
    
    // scionPrinter->printLog(IH, (char *)"BS(%s)'s src DAG: %s\n", m_AD.c_str(), src.c_str());

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
    xiah.set_plen(data_length + thdr->hlen()); // XIA payload = transport header + transport-layer data

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
