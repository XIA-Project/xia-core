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

/*
	scionrouter.cc
*/

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "define.hh"
/*change this to corresponding header*/
#include"scionrouter.hh"

// Tenma, added for AESNI
#ifdef ENABLE_AESNI
#include <intel_aesni/iaesni.h>
#endif

CLICK_DECLS
/*
    SCIONRouter::configure
    - click configureation function
*/
int SCIONRouter::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "AID", cpkM, cpUnsigned64, &m_uAid, //AID is same as the AID in the topology file
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile, 
        cpEnd) <0){
    }

    return 0;
}


/*
    SCIONRouter::initialize
    initializes variables
*/
int SCIONRouter::initialize(ErrorHandler* errh){

#ifdef ENABLE_AESNI
	if(!check_for_aes_instructions()) {
		printf("scion router did not support AESNI instructions.\n");
		exit(-1);
	}
#endif

    initVariables(); 
	initOfgKey();
	updateOfgKey();
    scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile);
    _timer.initialize(this); 
	//SL: schedule timer for IFID requests after all routers are initialized
	//however, this should be performed regularly to check the neighbor status
    _timer.schedule_after_sec(2);

    return 0;
}

/*SL: Initialize Opaque Field Generation (OFG) key */
bool SCIONRouter::initOfgKey() {
	time_t currTime;
	time(&currTime);
	//SL: to deal with the case that TDC BS initiates PCB before the BS 
	//of this AD starts
	currTime -= 300; //back 5minutes

	m_currOfgKey.time = currTime;
    memcpy(m_currOfgKey.key, &m_uMkey, OFG_KEY_SIZE);

#ifdef ENABLE_AESNI
	if(AESKeyExpand((UCHAR*)m_currOfgKey.key, &m_currOfgKey.aesnikey, OFG_KEY_SIZE_BITS)==NULL)
	{
		printf("Enc Key setup failure by AESKeyExpand\n");
		return SCION_FAILURE;
	}
#else
	int err;
	if(err = aes_setkey_enc(&m_currOfgKey.actx, m_currOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}
#endif
	return SCION_SUCCESS;
}

/*SL: 
	SCIONRouter::updateOfgKey
	Update OFG key when a new key is received from CS
*/
bool SCIONRouter::updateOfgKey() {
	//This function should be updated...
	m_prevOfgKey.time = m_currOfgKey.time;
	memcpy(m_prevOfgKey.key, m_currOfgKey.key, OFG_KEY_SIZE);

#ifdef ENABLE_AESNI
	if(AESKeyExpand((UCHAR*)m_prevOfgKey.key, &m_prevOfgKey.aesnikey, OFG_KEY_SIZE_BITS)==NULL)
	{
		printf("Enc Key setup failure by AESNI KeyExpand function.\n");
		return SCION_FAILURE;
	}
#else
	int err;
	if(err = aes_setkey_enc(&m_prevOfgKey.actx, m_prevOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Prev. Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}
#endif
	return SCION_SUCCESS;
}


/*
    SCIONRouter::parseTopology
    parses topology file using TopoParser
*/
void SCIONRouter::parseTopology(){
    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
    parser.parseRouters(m_routers);
}

/* SLT:
	SCIONRouter::constructIfid2AddrMap
	Construct the ifid2addr map based on the information read from the topology file
	Note: an ingress router should be able to forward a packet to the egress router 
	that owns the egress interface ID specified in the opaque field
*/
void
SCIONRouter::constructIfid2AddrMap() {
	std::multimap<int, RouterElem>::iterator itr;
	for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
		if(itr->second.aid == m_uAid) {
			m_Addr = itr->second.addr;
			//SLA: configure the address of its own interface
			//this will be used to 
			//(1) add outer header (e.g., IP header or XIA header) or
			//(2) peel off outer header if exists 
			int idx = ifid2port.find(itr->second.interface.id)->second;
			m_vPortInfo[idx]->addr = itr->second.interface.addr;
			m_vPortInfo[idx]->to_addr = itr->second.interface.to_addr;
			
			#ifdef _SL_DEBUG_RT
			if(itr->second.interface.addr.getType() == HOST_ADDR_IPV4) {
				in_addr sa, da;
				sa.s_addr = itr->second.interface.addr.getIPv4Addr();
				da.s_addr = itr->second.interface.to_addr.getIPv4Addr();
				
				char sas[INET_ADDRSTRLEN], das[INET_ADDRSTRLEN];

				inet_ntop(AF_INET, &sa.s_addr, sas,INET_ADDRSTRLEN);
				inet_ntop(AF_INET, &da.s_addr, das,INET_ADDRSTRLEN);

				printf("Router(%llu:%llu): IF=%d,  IPV4: SRC/DES IFID2ADDR: %s,\t%s\n", 
					m_uAdAid, m_uAid, itr->second.interface.id, sas,das);
			}
			#endif
		}

		ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interface.id, itr->second.addr));
	}
}


/*
    SCIONRouter::initVariables
    Initializes non-click, SCION router variables
	read from the topology file
*/
void SCIONRouter::initVariables(){
    Config config;
    //1. read configuration file
	config.parseConfigFile((char*)m_sConfigFile.c_str());

	//2. initialize variables based on the configuration file
    config.getOfgmKey((char*)m_uMkey);
    config.getLogFilename(m_csLogFile);
    ifid_set = config.getIFIDs();
    m_uAdAid = config.getAdAid();
    m_uTdAid = config.getTdAid();
    m_iLogLevel = config.getLogLevel();
    m_iCore = config.getIsCore();
    m_iNumIFID=ifid_set.size();
	//SL: this router's IFID to port mapping
    mapIFID2Port();
    
	//3. read AD topology information and setup forwarding
	//information (table?)
    parseTopology();
	//SL: all internal AD routers' IFID to Address mapping
	constructIfid2AddrMap();
	
	//4. initialize IPEncap for necessary interfaces
	//SL: currently support IPv4
	initializeOutputPort();
}

/*
	SCIONRouter::mapIFID2Port
	map its own port to the corresponding interface ID
	note: a router (with a single address) can have multiple interfaces
*/
void SCIONRouter::mapIFID2Port(){
    std::vector<uint16_t>::iterator itr;
    int ctr=1;
	portInfo * p = new portInfo;
	p->addr = m_Addr; //internal address (inside AD)
	p->m_bInitialized = true;

	//port 0 is connected to scion switch
	//yet it can be used for internal IP encapsulation
	m_vPortInfo.push_back(p);

    for(itr=ifid_set.begin();itr!=ifid_set.end();itr++){
		p = new portInfo;
		p->ifid = *itr;
		m_vPortInfo.push_back(p);
		ifid2port.insert(std::pair<uint16_t,int>(*itr,ctr++));
    }

	assert(m_vPortInfo.size() == (m_iNumIFID+1));
}


/*
	SCIONRouter::initializeOutputPort
	prepare IP header for IP encapsulation
	if the port is assigned an IP address
*/
void SCIONRouter::initializeOutputPort() {
	m_pIPEncap = new SCIONIPEncap [m_iNumIFID+1];

	//1. Initialize port 0; i.e., prepare internal communication
	if(m_Addr.getType() == HOST_ADDR_IPV4)
		m_pIPEncap[0].initialize(m_Addr.getIPv4Addr());

	//2. Initialize other ports connecting neighbor ADs

	for(int i = 1; i<m_iNumIFID+1; i++) {
		if(m_vPortInfo[i]->addr.getType() == HOST_ADDR_IPV4){
			if(m_vPortInfo[i]->to_addr.getType() == HOST_ADDR_IPV4){
				m_pIPEncap[i].initialize(m_vPortInfo[i]->addr.getIPv4Addr(),
					m_vPortInfo[i]->to_addr.getIPv4Addr());
			} else
				m_pIPEncap[i].initialize(m_vPortInfo[i]->addr.getIPv4Addr());
		}
	}
}

/*
    SCIONRouter::run_timer
    runs periodically and ask each neighbor AD edge router for its IFID. 
	this is used to diagnose whether a neighbor router is up or down.
*/
void SCIONRouter::run_timer(Timer* timer){

    int numPort = m_iNumIFID;
    map<int, uint16_t>::iterator itr;

	for(int i=1; i<m_vPortInfo.size(); i++){
	
		if(m_vPortInfo[i]->m_bInitialized)
			continue;

		//SL: Two IFIDs of connected routers will be used for matching
		//Hence, the IFID size should be reserved for both of them
		uint8_t hdrLen = COMMON_HEADER_SIZE + SCION_ADDR_SIZE*2;
		uint8_t totalLen = hdrLen+sizeof(IFIDRequest);
		uint8_t buf[totalLen];
		memset(buf, 0, totalLen);

		scionHeader hdr;

		HostAddr src = HostAddr(HOST_ADDR_SCION, m_uAdAid);
		HostAddr dst = HostAddr(HOST_ADDR_SCION, (uint64_t)0);

		hdr.src = src;
		hdr.dst = dst;

		hdr.cmn.type = IFID_REQ;
		hdr.cmn.hdrLen = hdrLen;
		hdr.cmn.totalLen = totalLen; 	//a border router replies two ifids;
										//i.e., its own and its neighbor

		SPH::setHeader(buf,hdr);
		IFIDRequest * ir = (IFIDRequest *)(buf+hdrLen);
		ir->request_id = m_vPortInfo[i]->ifid;

		sendPacket(buf, totalLen, i);
    }
	//SLT: currently, this is called only once.
	//run periodically if first run fails
	//or if connected router has failed
    _timer.schedule_after_sec(IFID_REQ_INT);
}

/*
    SCIONRouter::push
    main routine that handles incoming packets
*/
//SLT: This should be changed...
//either Pull from a buffer
//or internal buffer in the pull and timer to handle the current code
//To-Do: multi-threaded handling is deemed necessary
void SCIONRouter::push(int port, Packet* pkt){
    
	uint8_t * s_pkt = (uint8_t *) pkt->data();

	//remove IP header if it exists
	if(m_vPortInfo[port]->addr.getType() == HOST_ADDR_IPV4) {
		struct ip * p_iph = (struct ip *)s_pkt;
		struct udphdr * p_udph = (struct udphdr *)(p_iph+1);
		#ifdef _SL_DEBUG_RT
		char sas[INET_ADDRSTRLEN], das[INET_ADDRSTRLEN];

		inet_ntop(AF_INET, &p_iph->ip_src.s_addr, sas,INET_ADDRSTRLEN);
		inet_ntop(AF_INET, &p_iph->ip_dst.s_addr, das,INET_ADDRSTRLEN);
		printf("Router(%llu:%llu):IF(%d): IP Sender IP: %s, Recipient IP: %s\n", 
			m_uAdAid, m_uAid, m_vPortInfo[port]->ifid, sas,das);
		#endif
		//SLA:
		//IP checksum verification routine should be added header...
		if(p_iph->ip_p != SCION_PROTO_NUM || ntohs(p_udph->dest) != SCION_PORT_NUM) {
			//printf("PROTO:%d, PORT:%d\n",p_iph->ip_p, p_udph->dest);
			pkt->kill();
			return;
		}
		s_pkt += UDPIPHDR_LEN;
	}

    //copy the data of the packet and kill (i.e., release memory) the incoming packet
	scionCommonHeader *cmn = (scionCommonHeader *)s_pkt;
    int type = cmn->type; 

    uint16_t packetLength = cmn->totalLen; 
    uint8_t packet[packetLength];
    memset(packet, 0, packetLength);
    memcpy(packet, s_pkt, packetLength);
    pkt->kill();

    /*
        If the packet is from switch and it isn't AID request packet,
        then look at the destination AID and forward the packet to 
        the corresponding port. 
    */
    
	//SL: changed to source and destination addresses instead of AID
	//source and destination addresses have variable length
	HostAddr src = SPH::getSrcAddr(packet); 
    HostAddr dst = SPH::getDstAddr(packet);
   
	scionPrinter->printLog(IH,type,0,src,dst,"%u,RECEIVED\n",packetLength); 
    
	//SL: change the processing order: most frequent first...
	/*1. Data packet first*/
	if(type == DATA) {//process data packet first...
		if(!m_vPortInfo[port]->m_bInitialized)
			return;
		if(port) {//data packet from neighbor ADs  
			#ifdef _SL_DEBUG_GW
			printf("Router (%llu:%llu): forward data packet\n", m_uAdAid, m_uAid);
			#endif
			forwardDataPacket(port, packet);
		} else { //data packet from switch; i.e., port == 0
			uint8_t srcLen = cmn->srcLen; 
			uint8_t dstLen = cmn->dstLen; 
			uint8_t pCurrOF = cmn->currOF; 
			uint8_t type = cmn->type; 
			uint8_t *of = SPH::getCurrOF(packet);

			if(pCurrOF == (srcLen+dstLen)) {//Skip at the router of the origin AD
				#ifdef _SL_DEBUG_GW
				printf("Router (%llu:%llu): origin -- forward data packet: pCurrOF = %d\n", 
					m_uAdAid, m_uAid, pCurrOF);
				#endif
				forwardDataPacket(port, packet);
			} else {
				if(*of == TDC_XOVR){ //TDC Crossover...
					SPH::setTimestampPtr(packet,pCurrOF);
					SPH::setDownpathFlag(packet);
					SPH::increaseOFPtr(packet,1);
				} else if (*of == NON_TDC_XOVR){ //Non-TDC Crossover...
					SPH::setTimestampPtr(packet,pCurrOF);
					SPH::setDownpathFlag(packet);
					SPH::increaseOFPtr(packet,2);
				}
				writeToEgressInterface(port, packet);
			}
		}
	
	/*2. All other control packets*/
	} else {
		processControlPacket(port, packet);
	} //end of control packet block
}

/*
	SLN:
	normal forwarding to the next hop AD
*/
int
SCIONRouter::normalForward(uint8_t type, int port, SPacket * packet, uint8_t isUppath) {

	scionCommonHeader * cmn = (scionCommonHeader *)packet;
    uint16_t packetLength = cmn->totalLen; 

	//verify Opaque Field
	if(verifyOF(port, packet)==SCION_FAILURE){
		#ifdef _SL_DEBUG_RT
		printf("Router (%llu:%llu): Mac verification failure.\n", m_uAdAid, m_uAid);
		#endif
		scionPrinter->printLog(EH,type,"MAC CHECK FAILED3\n");
		
		return SCION_FAILURE;
	}
	
	//if passed then send to next hop
	int outport=0; //to switch...

	uint8_t * nof = SPH::getCurrOF(packet);
	opaqueField * pOF = (opaqueField *) nof;

	uint16_t iface;
	
	if(isUppath)
		iface = pOF->ingressIf;
	else
		iface = pOF->egressIf;
	
	//SLT:
	//this part should be removed after having TDC servers set the downpath flag
	//if(cmn->type==PATH_REP || cmn->type==CERT_REP || cmn->type==ROT_REP){
	//	iface = pOF->egressIf;
	//}

	int fwd_type = 0; //forwarding type...

	if(!port) { //from switch, then forward to the connected router...
		
		map<uint16_t,int>::iterator itr;
		if((itr = ifid2port.find(iface)) == ifid2port.end()){
			//no port is found... this would not happen
			printf("here iface = %lu\n", iface);
			return SCION_FAILURE;
		}
		outport = itr->second;
		//leaving an AD, hence increase OF pointer.
		SPH::increaseOFPtr(packet,1);

	} else { //set destination address to that of the egress router (ifid)
		//SL: this is wrong. Src Addr should not be changed by router
		//However, let's think about when this is necessary
		if(type==CERT_REQ||type==ROT_REQ||type==CERT_REP||type==ROT_REP){
			HostAddr dstAddr = m_servers.find(CertificateServer)->second.addr;  
			SPH::setDstAddr(packet, dstAddr);
			fwd_type = TO_SERVER;
		}else if(iface) { //Non-TDC
			//SL: this part seems not reaching.. need to check
			if(type != DATA) {
				std::map<uint16_t,HostAddr>::iterator itr = ifid2addr.find(iface);
				SPH::setDstAddr(packet, itr->second);
			} else {
				fwd_type = TO_ROUTER;
			}
		} else { //TDC
			if(type == PATH_REG || type == PATH_REQ || type == PATH_REP) {
				SPH::setDstAddr(packet, m_servers.find(PathServer)->second.addr);
			}
		}
	}
	//otherwise, send to switch...
	
	sendPacket(packet, packetLength, outport, fwd_type);

	return SCION_SUCCESS;
}

int
SCIONRouter::crossoverForward(uint8_t type, uint8_t info, int port, SPacket * packet, uint8_t isUppath) {
	//several cases for cross over case
	//1. TDC crossover
	//2. Shortcut
	//3. On-path crossover
	//4. peering link
	scionCommonHeader * cmn = (scionCommonHeader *)packet;
    uint16_t packetLength = cmn->totalLen; 
	//1 TDC crossover

	//MSB of the info field should be 1 (i.e., special opaque field)
	uint8_t pCurrOF;
	int fwd_type = 0;

	switch(info) {
	case TDC_XOVR:{//if 2nd MSB in the special OF is 0, it's TDC path
		#ifdef _SL_DEBUG_GW
		printf("TDC Crossover reached\n");
		#endif
		//Core up path MAC verification
		if(verifyOF(port, packet)!=SCION_SUCCESS){
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ TDC\n");
			return SCION_FAILURE;
		}

		//1.1 Packet arrives at TDC using up-path
		if(isUppath){
			//1.1.1 PATH_REG and PATH_REQ goes to TDC path server
			if(type==PATH_REG || type == PATH_REQ){
		
				//get path server address
				HostAddr psAddr = m_servers.find(PathServer)->second.addr;
				//now CurrOF* points to the start of payload (i.e., #OF * 8B == CurrOF)
				SPH::increaseOFPtr(packet,1);
				//set destination address	
				SPH::setDstAddr(packet, psAddr);
				fwd_type = TO_SERVER;
					
			//1.1.2 other packets are assumed to be not destined to TDC (for now)
			} else {
				
				//increase one opaque field pointer
				uint16_t ifid;
				//need to implement both getIngressIF & getEgressIF
				//by default, the functions return the current ingress/egress IFs
				//second argument indicates the opaque field that is displaced 
				//by the number from the current OF
				
				uint8_t nSpecialOF = 1; //by default, # of special OF at TDC is 1
				//SLT:
				//getSpecialOFNum should be added.
				//nSpecialOF = getSpecialOFNum(packet, pCurrOF);
				//if packet traverses multiple TDC, then it should be added to 
				//find out the next regular OF.
				
				SPH::increaseOFPtr(packet, 1); //the next OF is the special OF
				fwd_type = TO_ROUTER;
			}
		//1.2 Packet leaves from TDC to down-path
		} else{ //Downpath
			//if passed then set packet header and send to next child
			SPH::increaseOFPtr(packet,1);
			fwd_type = TO_ROUTER;
		}
		sendPacket(packet, packetLength, PORT_TO_SWITCH, fwd_type);
		}
		break;

	case NON_TDC_XOVR:{
		#ifdef _SL_DEBUG_GW
		printf("Non-TDC Crossover reached\n");
		#endif
		//MAC verification
		if(verifyOF(port, packet) != SCION_SUCCESS){
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ Crossover AD\n");
			return SCION_FAILURE;
		}
		//2.1 Packet arrive at crossover AD through up-path
		if(isUppath){
			//increase one opague field pointer
			uint16_t ifid;
			// if more than one special OF is present, 
			//the offset should be increased by the number of special OFs -1.
			
			SPH::increaseOFPtr(packet, 2); //move pointer to the special OF
		//2.2 Packet leaves from crossover AD to down-path
		} else{ //Downpath
			//the first downpath OF is for the MAC verification
			SPH::increaseOFPtr(packet,1);
			//if passed then set packet header and send to next child
			SPH::increaseOFPtr(packet,1);
		}
		sendPacket(packet, packetLength, PORT_TO_SWITCH,TO_ROUTER);
		} 
		break;

	case INPATH_XOVR:{
		//MAC verification
		if(verifyOF(port, packet) != SCION_SUCCESS){
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ On-Path\n");
			return SCION_FAILURE;
		}
		//a) update TS
		int isRegular = 1;
		int hops = 0;
		while(isRegular) {
			pCurrOF = SPH::increaseOFPtr(packet,1); 
			isRegular = SPH::isRegularOF(packet, pCurrOF);
			hops++;
		}

		SPH::setTimestampPtr(packet, pCurrOF);
		//b) move pointer to the destination AD (i.e., itself) on the downpath
		SPH::increaseOFPtr(packet,hops); //symmetric on TS 
		//c) another MAC verification for the downpath
		if(verifyOF(port, packet) != SCION_SUCCESS){
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ On-Path Out\n");
			printf("MAC check 4\n");
			return SCION_FAILURE;
		}
		//d) send packet to the destination host
		sendPacket(packet, packetLength, PORT_TO_SWITCH,TO_ROUTER);
		}
		break;

	case INTRATD_PEER:{
		//current OF is added for the previous AD's MAC verification
		//a) move pointer to the peering link OF
		if(isUppath)
			SPH::increaseOFPtr(packet, 1); 
		//b) MAC verification
		if(verifyOF(port, packet) != SCION_SUCCESS){
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ Peering Link Up\n");
			return SCION_FAILURE;
		}

		//c) move OF pointer by 2 since the next OF is for the current MAC verification
		if(!isUppath)
			SPH::increaseOFPtr(packet, 2); //move pointer to the special OF

		sendPacket(packet, packetLength, PORT_TO_SWITCH,TO_ROUTER); //this should be delivered to the egress IF of TDC
		}
		break;

	case INTERTD_PEER:
		//SL: Not implemented yet; however this routine would be similar to that of INTRATD_PEER
		break;

	default: 
		printf("Router (%llu:%llu): Unknown crossover type: %x\n", m_uAdAid,m_uAid,info);
		break;
	}
}


/** 
SL:
General packet forwarding using opaque field
*/
int SCIONRouter::forwardDataPacket(int port, SPacket * packet) {
	
	scionCommonHeader * cmn = (scionCommonHeader *)packet;
	uint8_t pCurrOF = cmn->currOF; //CurrOF*   
	uint8_t *currOF = SPH::getCurrOF(packet); 	//CurrOF   
	uint8_t oinfo = *currOF;

	uint8_t srcLen = cmn->srcLen; 
	uint8_t dstLen = cmn->dstLen; 
    uint8_t type = cmn->type; 
	
	oinfo = oinfo >> 7;
	int isRegular = !oinfo; //check if MBS is 0
	
	//for the TDC AD edge router
	while(!isRegular) { //special opaque field; i.e., timestamp
		//1. update TS*
		SPH::setTimestampPtr(packet, pCurrOF);
        
       	//HACK!!!!!! SJ: TODO
		//2. Set the up-path/down-path flag
		if((type!=PATH_REP && type!=CERT_REP && type!=ROT_REP) && pCurrOF == (srcLen + dstLen)){
			SPH::setUppathFlag(packet);
       	}else{
			SPH::setDownpathFlag(packet);
       	}

		//3. increase the OF pointer to the next one
		pCurrOF = SPH::increaseOFPtr(packet, 1);
		isRegular = SPH::isRegularOF(packet, pCurrOF);
		//SL: if a sepecial OF consists of multiple OFs, it should be handled here...
	} 

	//normal opaque field handling for forwarding
	uint8_t pTS = cmn->timestamp; //TS*
	uint8_t info = SPH::getTimestampInfo(packet, pTS); //Information field in the TS opaque field
	uint8_t of_type = SPH::getOFType(packet);
	uint32_t TS = SPH::getTimestamp(packet);	//Timestamp

	//1 bit shift left. 1st bit should be 0 since it's a regular OF
	of_type = of_type << 1; 

	//2nd bit is 1 if another opaque field follows
	//Currently we don't have this case, yet need to process it if needed
	while(of_type & MASK_MSB){ 
		//do some action and increase OF pointer
		pCurrOF = SPH::increaseOFPtr(packet,1);
		of_type = SPH::getOFType(packet);
		of_type = of_type << 1; //1 bit shift left, 1st bit should be 0 since it's a regular OF
	}

	of_type = of_type << 1;
	
	//SL: let's extract information from the flag instead of using the flag directly
	//e.g., SPH::isUpPath(packet)
	uint8_t flag = SPH::getFlags(packet);
	uint8_t isUppath = flag & MASK_MSB;
	
	//if downpath and peer OF, increase one more OFPtr
	//since the current OF is added for MAC verification
	if(!isUppath && (pCurrOF==pTS+OPAQUE_FIELD_SIZE) && (info&PEER_XOVR))
	{
		#ifdef _SL_DEBUG_RT
		printf("peering rec. reached\n");
		#endif
		pCurrOF = SPH::increaseOFPtr(packet, 1);
	}

	//now the regular OF field is being processed
	//if 3rd bit is 1 it's a crossover point; otherwise, 
	//the router is on the path, so forwards the packet to the egress IF if MAC verification passes.
	//1. normal forwarding (non-crossover)
	int ret;

	if(!(of_type & MASK_MSB)) {
        if((ret = normalForward(type,port,packet,isUppath)) != SCION_SUCCESS)
			return ret;
	//2. Crossover forwarding; router needs to handle multiple OFs
	} else { // cross over forwarding
		if((ret = crossoverForward(type,info,port,packet,isUppath)) != SCION_SUCCESS)
			return ret;

	}
    return SCION_SUCCESS;

}

/**
SL:
** Write a packet to the egress interface
*/
int SCIONRouter::writeToEgressInterface(int port, SPacket * packet) {
	
	scionCommonHeader * cmn = (scionCommonHeader *)packet;
	uint16_t ifid = SPH::getOutgoingInterface(packet);
	uint8_t type = cmn->type; 
    uint16_t packetLength = cmn->totalLen; 
	uint8_t pTS = cmn->timestamp; //TS*
	uint8_t info = SPH::getTimestampInfo(packet, pTS); //Information field in the TS opaque field
	
	//SL: Egress router increase the OF Ptr
	//We have to decide whether the egress router would verify OF again or not.
	//Currently the egress router trusts the ingress router's verification
	SPH::increaseOFPtr(packet,1);//leaving an AD, hence increase OF pointer.
	if(info &PEER_XOVR){ //Peer: increase another OF pointer since the next OF is just for verification
		#ifdef _SL_DEBUG_GW
		printf("Crossing peering link\n");
		#endif
		SPH::increaseOFPtr(packet,1);
	}

    int outputPort = 0;

	map<uint16_t,int>::iterator itr;
	if((itr = ifid2port.find(ifid)) == ifid2port.end()) //no port is found... this would not happen
		return SCION_FAILURE;
				
	outputPort = itr->second;
	
	if(!outputPort){
		scionPrinter->printLog(EH,type,"output port1 not found\n");
		return SCION_FAILURE;
	}
	
	sendPacket(packet, packetLength, outputPort);
	return SCION_SUCCESS;
}

/**
SL:
Process non-data, control packets here
Router forward packets to corresponding servers (packets from other ADs) 
or the connected router (packets from SCIONSwitch, i.e., port == 0).
*/
int SCIONRouter::processControlPacket(int port, SPacket * packet)
{
	scionCommonHeader * cmn = (scionCommonHeader *)packet;
    uint8_t type = cmn->type; 
    uint16_t packetLength = cmn->totalLen; 

    //SL: for special control packets that need destination ID to be set to its own AID
	//for logging purpose...
	if(type==AID_REQ||type==IFID_REQ||type==IFID_REP||type==BEACON){
		HostAddr dst = HostAddr(HOST_ADDR_SCION,m_uAdAid);
    }
		
   	/*1 BEACON: Beacon received from the upstream*/
	if(type==BEACON){ //PCB is the most frequent packet except DATA packets
		if(!m_vPortInfo[port]->m_bInitialized)
			return SCION_FAILURE;
        
		//PCB from Beacon Server.
		if(port == PORT_TO_SWITCH) {
   			uint16_t ifid = SCIONBeaconLib::getInterface(packet);
			int outputPort=0;

			map<uint16_t,int>::iterator itr;
			if((itr = ifid2port.find(ifid)) == ifid2port.end()) {//no port is found... this would not happen
           		scionPrinter->printLog(EH,type,"output port2 not found\n");
				return SCION_FAILURE;
			}
				
			outputPort = itr->second;
			sendPacket(packet,packetLength,outputPort);
		} else {
			//set the destination to the beacon server
			HostAddr bsAddr = m_servers.find(BeaconServer)->second.addr;
			SPH::setDstAddr(packet, bsAddr);

			//SL: set ingress interface id to let BS know the incoming interface.
			SCIONBeaconLib::setInterface(packet, getIFID(port));
			if(SCIONBeaconLib::getInterface(packet)==0){
				scionPrinter->printLog(EH,type,"Beacon Server ifid not found\n");
				return SCION_FAILURE;
			}

			//sends packet to the beacon server
			sendPacket(packet, packetLength, PORT_TO_SWITCH,TO_SERVER);
		}
   	
   	}else {
		switch(type) {
		case IFID_REQ:{
			/*2 IFID_REQ: IFID request packet from the neighboring AD edge routers*/

			//append the IFID
			uint16_t ifid = m_vPortInfo[port]->ifid;
			//SL: this should be revised... let's don't use pointer directly...
			uint8_t hdrLen = cmn->hdrLen; 
			IFIDRequest * ir = (IFIDRequest *)(packet+hdrLen);
			ir->reply_id = ifid;
			
			HostAddr dstAddr = SPH::getSrcAddr(packet);
			//sets the packet header
			//don't need to construct a header; 
			//only need to change the type, src/dst addresses
			SPH::setType(packet, IFID_REP);
			//SL: this should be handled differently
			//currently only SCION address can be initialized
			HostAddr srcAddr(HOST_ADDR_SCION,m_uAdAid);
			SPH::setSrcAddr(packet,srcAddr);//set src addr to ADAID (for logging).
			SPH::setDstAddr(packet,dstAddr); //set dst addr to that of requester (i.e., src)

			//return the packet
			sendPacket(packet, packetLength, port);
			}
			break;

		case IFID_REP:{
			/*3 IFID_REP: IFID reply from the neighbor AD edge router
			- this packet represents the corresponding interface is active
			so the router sends this to the beacon server.
			*/
			
			//IFIDNEW
			//puts local ifid and neighbor ifid to the packet
			//SL: this should be modified
			uint8_t hdrLen = cmn->hdrLen; //SPH::getHdrLen(packet);
			uint16_t newIfid = *(uint16_t*)(packet+hdrLen);
			uint16_t ifid = m_vPortInfo[port]->ifid;
			//SL: this ifid_map is not used currently.
			//need to check if this is necessary.
			ifid_map.insert(std::pair<uint16_t, uint16_t>(ifid, newIfid));
			m_vPortInfo[port]->m_bInitialized = true;
			//SL: 2nd ifid is the local IFID
			//this part is just to make sure the requesting router's ID is correct..
			//IFIDRequest * ir = (IFIDRequest *)(packet+hdrLen);
			//ir->request_id = ifid;

			//SLT: need to consider multiple BSs later...
			HostAddr bsAddr = m_servers.find(BeaconServer)->second.addr;
			//note: addDstAddr would either insert a dest addr or replace the current one with the new one.
			HostAddr srcAddr(HOST_ADDR_SCION, m_uAid);
			SPH::setSrcAddr(packet, srcAddr);
			SPH::setDstAddr(packet, bsAddr);
		
			//sends to beacon server
			sendPacket(packet, packetLength, PORT_TO_SWITCH,TO_SERVER);
			}
			break;

		case AID_REQ: {
    		/*4 AID_REQ: AID Request from the switch*/
			SPH::setType(packet, AID_REP);
			HostAddr addr(HOST_ADDR_SCION,m_uAid);
			SPH::setSrcAddr(packet, addr);

			sendPacket(packet, packetLength, PORT_TO_SWITCH);
			}
			break;

		default:
			/*5 Forward all other control packets from neighbor AD using opaque field*/
			//in general, this would be reached when type == CERT_REQ || ROT_REQ...
			if(!m_vPortInfo[port]->m_bInitialized)
				return SCION_FAILURE;
			if(forwardDataPacket(port, packet)) {
				//if this part is reached, something wrong happened.
				printf("Router (%llu:%llu): Something wrong happened in forwarding a control packet\n"
					,m_uAdAid, m_uAid);
				return SCION_FAILURE;
			}
			break;
		}

    }
}

/*
    SCIONRouter::getIFID
    get IFID that represents this click port number
*/
uint16_t SCIONRouter::getIFID(int port){
    return m_vPortInfo[port]->ifid;
}

#ifdef ENABLE_AESNI
bool SCIONRouter::getOfgKey(uint32_t timestamp, keystruct &ks)
#else
bool SCIONRouter::getOfgKey(uint32_t timestamp, aes_context &actx)
#endif
{
	if(timestamp > m_currOfgKey.time) {
#ifdef ENABLE_AESNI
		ks = m_currOfgKey.aesnikey;
#else
		actx = m_currOfgKey.actx;
#endif
	} else {
#ifdef ENABLE_AESNI
		ks = m_prevOfgKey.aesnikey;
#else
		actx = m_prevOfgKey.actx;
#endif
	}
    return SCION_SUCCESS;

#ifndef ENABLE_AESNI

	//SL: following part is left to compare performance later.
	//////////////////////////////////////////////////////////
    std::map<uint32_t, aes_context*>::iterator itr;

    //aes_context actx;
	//SL: check if this necessary
    memset(&actx, 0, sizeof(aes_context));

    //when the key table is full
    while(m_OfgAesCtx.size()>KEY_TABLE_SIZE){
		itr = m_OfgAesCtx.begin();
		delete itr->second;
        m_OfgAesCtx.erase(itr);
    }

    //if key for the timestmpa is not found. 
    //if((itr=key_table.find(timestamp)) == key_table.end()){
    if((itr=m_OfgAesCtx.find(timestamp)) == m_OfgAesCtx.end()){

        //concat timestamp with the ofg master key.
        uint8_t k[SHA1_SIZE];
        memset(k, 0, SHA1_SIZE);
        memcpy(k, &timestamp, sizeof(uint32_t));
        memcpy(k+sizeof(uint32_t), m_uMkey, OFG_KEY_SIZE);

        //creates sha1 hash 
        uint8_t buf[SHA1_SIZE];
        memset(buf, 0 , SHA1_SIZE);
        sha1(k, TS_OFG_KEY_SIZE, buf);

        ofgKey newKey;
        memset(&newKey, 0, OFG_KEY_SIZE);
        memcpy(newKey.key, buf, OFG_KEY_SIZE);

		//SL: OFG key aes context to avoid redundant key scheduling
		//store the aes_context in m_OfgAesCtx
		aes_context * pActx = new aes_context; 
		
		int err;
		if(err = aes_setkey_enc(pActx, newKey.key, OFG_KEY_SIZE_BITS)) {
			printf("Enc Key setup failure: %d\n",err*-1);
			return SCION_FAILURE;
		}

		m_OfgAesCtx.insert(std::pair<uint32_t, aes_context*>(timestamp, pActx));
		actx = *m_OfgAesCtx.find(timestamp)->second;

    }else{
		actx = *m_OfgAesCtx.find(timestamp)->second;
    }

    return SCION_SUCCESS;
#endif
}

/*
SL:
Check if interface ID of an opaque field matches with the current incoming interface
dir =1: uppath, dir = 0: downpath
*/
int SCIONRouter::verifyInterface(int port, SPacket * packet, uint8_t dir) {

	uint16_t in_ifid, e_ifid, ifid;
	uint8_t type = SPH::getType(packet);
    in_ifid = SPH::getIngressInterface(packet);
	e_ifid = SPH::getEgressInterface(packet);
	if((dir && e_ifid) || (!in_ifid && !dir)) //either on uppath (except the source AD) or on downpath at TDC
		ifid = e_ifid;
	else
		ifid = in_ifid;
		
	int inPort = 0;

	if(port > m_vPortInfo.size()) {
		scionPrinter->printLog(EH,type,"Interface verification failure: interface not found\n");
		return SCION_FAILURE;
	}
	
	if(m_vPortInfo[port]->ifid==ifid) {
		return SCION_SUCCESS;
	} else {
		scionPrinter->printLog(EH,type,"Interface verification failure: input port mismatch: ifid=%d, packet ifid=%d\n",
			m_vPortInfo[port]->ifid, ifid);
		return SCION_FAILURE;
	}
}

/****************************************************
    SCIONRouter:: MAC VERIFICATION FUNCTIONS
*****************************************************/
//SL:
//All verification is performed by this function
int SCIONRouter::verifyOF(int port, SPacket * packet) {
	
	uint8_t flag = SPH::getFlags(packet);
	uint8_t isUppath = flag & MASK_MSB;
    uint32_t timestamp = SPH::getTimestamp(packet);

	if(port){ //if packet is coming from an external AD 
	// port 0 is assigned to connect switch (i.e., internal)
		if(verifyInterface(port, packet, isUppath) == SCION_FAILURE){
		    printf("interface verification failed for port %d, isUppath %d\n", port, isUppath);
        	return SCION_FAILURE;
        }
	}

#ifdef ENABLE_AESNI
	keystruct ks;
	getOfgKey(timestamp,ks);
#else
	aes_context actx;
	getOfgKey(timestamp,actx);
#endif
	
	uint8_t * nof = SPH::getCurrOF(packet);
	uint8_t * ncof = SPH::getOF(packet,1);
	opaqueField * of = (opaqueField *)nof;
	opaqueField * chained_of = (opaqueField *) ncof;
	opaqueField no = opaqueField();


	//SL: getNextOF & getPreviousOF need to be implemented.
	//Use ::getOF(uint8_t * pkt, int offset)
	//offset = 1 => next OF, offset = -1 => previous OF
	if(isUppath){
		if(of->ingressIf) {//Non-TDC
    		ncof = SPH::getOF(packet,1);
    		chained_of = (opaqueField *) ncof;
		}else { //TDC
			chained_of = &no; //i.e., set to 0
		}
	//2. Down-path
    }else{
		if(of->ingressIf){ //Non-TDC
    		ncof = SPH::getOF(packet,-1);
    		chained_of = (opaqueField *) ncof;
		} else {//TDC
			chained_of = &no; //i.e., set to 0
		}
    }
    
	uint8_t exp = MASK_EXP_TIME & of->type; //last two bits of the type field carry the expiration time
#ifdef ENABLE_AESNI
	return SCIONBeaconLib::verifyMAC(timestamp, exp, of->ingressIf, of->egressIf, *(uint64_t *)&chained_of,of->mac,&ks);
#else
	return SCIONBeaconLib::verifyMAC(timestamp, exp, of->ingressIf, of->egressIf, *(uint64_t *)&chained_of,of->mac,&actx);
#endif
}

/*
    SCIONRouter::sendPacket
    create a click packet using the given data and send it out to the given port
*/
void SCIONRouter::sendPacket(uint8_t* data, uint16_t data_length, int port, int fwd_type){

	assert(data);
	scionCommonHeader *cmn = (scionCommonHeader *)data;
    uint8_t type = cmn->type; 
        
    HostAddr src = SPH::getSrcAddr(data); 
    HostAddr dst = SPH::getDstAddr(data);
    
	//SLA:
	//IPv4 handling here
	//if addrtype == IPv4, encap with IP header
	uint8_t ipp[data_length+UDPIPHDR_LEN]; //m_IPEncap.getHeaderLen();
	if(m_vPortInfo[port]->addr.getType() == HOST_ADDR_IPV4) {
		if(!port) { //internal forwarding...
			switch(fwd_type) {
			case TO_SERVER:
				if(m_pIPEncap[port].encap(ipp,data,data_length,SPH::getDstAddr(data).getIPv4Addr()) 
					== SCION_FAILURE)
					return;
			break;
			case TO_ROUTER:{
				uint16_t iface = SPH::getOutgoingInterface(data);
				std::map<uint16_t,HostAddr>::iterator itr = ifid2addr.find(iface);
				if(itr == ifid2addr.end()) {
					scionPrinter->printLog(EH,type, "Router(%llu:%llu): Address of interface (%d) not found\n",
						m_uAdAid, m_uAid, iface);
					return;
				}
				if(m_pIPEncap[port].encap(ipp,data,data_length,itr->second.getIPv4Addr()) == SCION_FAILURE)
					return;
			} break;
			default: break;
			}
			

		} else { //forwarding to neighbor AD (router)
			if(m_pIPEncap[port].encap(ipp,data,data_length) == SCION_FAILURE)
				return;
		}
		data = ipp;
	}

	scionPrinter->printLog(IH,type,0,src,dst,"%u,SENT\n",data_length); 
    WritablePacket *outPacket = Packet::make(DEFAULT_HD_ROOM, data, data_length,
        DEFAULT_TL_ROOM);
    output(port).push(outPacket);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONRouter)


