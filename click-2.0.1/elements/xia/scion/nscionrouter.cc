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
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include <sys/time.h>

#include "define.hh"
/*change this to corresponding header*/
#include"nscionrouter.hh"


CLICK_DECLS
/*
    NSCIONRouter::configure
    - click configureation function
*/
int NSCIONRouter::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "AID", cpkM, cpUnsigned64, &m_uAid,
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile, 
        cpEnd) <0){
    }

    return 0;
}


/*
    NSCIONRouter::initialize
    initializes variables
*/
int NSCIONRouter::initialize(ErrorHandler* errh){
    initVariables(); 
	initOfgKey();
	updateOfgKey();
    scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile);
    _timer.initialize(this); 
	//SL: schedule timer for IFID requests after all routers are initialized
	//however, this should be checked regularly to check the neighbor status
    _timer.schedule_after_sec(2);

    return 0;
}

/*SL: Initialize Opaque Field Generation (OFG) key */
bool NSCIONRouter::initOfgKey() {
	time_t currTime;
	time(&currTime);
	//SL: to deal with the case that TDC BS initiates PCB before the BS 
	//of this AD starts
	currTime -= 300; //back 5minutes

	m_currOfgKey.time = currTime;
    memcpy(m_currOfgKey.key, &m_uMkey, OFG_KEY_SIZE);

	int err;
	if(err = aes_setkey_enc(&m_currOfgKey.actx, m_currOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}

	return SCION_SUCCESS;
}

/*SL: 
	NSCIONRouter::updateOfgKey
	Update OFG key when a new key is received from CS
*/
bool NSCIONRouter::updateOfgKey() {
	//This function should be updated...
	m_prevOfgKey.time = m_currOfgKey.time;
	memcpy(m_prevOfgKey.key, m_currOfgKey.key, OFG_KEY_SIZE);

	int err;
	if(err = aes_setkey_enc(&m_prevOfgKey.actx, m_prevOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Prev. Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}

	return SCION_SUCCESS;
}


/*
    NSCIONRouter::parseTopology
    parses topology file using TopoParser
*/
void NSCIONRouter::parseTopology(){
    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
    parser.parseRouters(m_routers);
}

/* SLT:
	NSCIONRouter::constructIfid2AddrMap
	Construct the ifid2addr map based on the information read from the topology file
	Note: an ingress router should be able to forward a packet to the egress router 
	that owns the egress interface ID specified in the opaque field
*/
void
NSCIONRouter::constructIfid2AddrMap() {
	std::multimap<int, RouterElem>::iterator itr;
	for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
		ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interfaceID, itr->second.addr));
	}
}


/*
    NSCIONRouter::initVariables
    Initializes non-click, SCION router variables
	read from the topology file
*/
void NSCIONRouter::initVariables(){
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
    mapIFID2Port();
    
	//3. read AD topology information and setup forwarding
	//information (table?)
    parseTopology();
	constructIfid2AddrMap();
}


/*
    NSCIONRouter::run_timer
    runs periodically and ask each neighbor AD edge router for its IFID. 
	this is used to diagnose whether a neighbor router is up or down.
*/
void NSCIONRouter::run_timer(Timer* timer){

    int numPort = m_iNumIFID;
    map<int, uint16_t>::iterator itr;

    for(itr=port2ifid.begin();itr!=port2ifid.end();itr++){
	//IFIDNEW
	#ifdef _SL_DEBUG
	//SLT:
	printf("Router (%llu): about to send IFID_REQ to port %d\n", m_uAdAid, itr->first);
	#endif
	
	//SL: Two IFIDs of connected routers will be used for matching
	//Hence, the IFID size should be reserved for both of them
	uint8_t hdrLen = COMMON_HEADER_SIZE + SCION_ADDR_SIZE*2;
	uint8_t totalLen = hdrLen+IFID_SIZE*2;
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

	SCIONPacketHeader::setHeader(buf,hdr);

    Packet* outPacket = Packet::make(DEFAULT_HD_ROOM, buf,
            totalLen, DEFAULT_TL_ROOM);
    output(itr->first).push(outPacket);
	#ifdef _SL_DEBUG
	//SLT:
	printf("Router (%llu): IFID_REQ to port %d sent out\n", m_uAdAid, itr->first);
	#endif

    }
	//SLT: currently, this is called only once.
	//run periodically if first run fails
	//or if connected router has failed
}

/*
	NSCIONRouter::mapIFID2Port
	map its own port to the corresponding interface ID
	note: a router (with a single address) can have multiple interfaces
*/
void NSCIONRouter::mapIFID2Port(){
    std::vector<uint16_t>::iterator itr;
    int ctr=1;
    for(itr=ifid_set.begin();itr!=ifid_set.end();itr++){
        port2ifid.insert(std::pair<int, uint16_t>(ctr,*itr));
		ifid2port.insert(std::pair<uint16_t,int>(*itr,ctr++));
    }
}

/*
    NSCIONRouter::push
    main routine that handles incoming packets
*/
//SLT: This should be changed...
//either Pull from a buffer
//or internal buffer in the pull and timer to handle the current code
//To-Do: multi-threaded handling is deemed necessary
void NSCIONRouter::push(int port, Packet* pkt){
    
    //copy the data of the packet and kill (i.e., release memory) the incoming packet
    int type = SCIONPacketHeader::getType((uint8_t*)pkt->data());
    uint16_t packetLength = SCIONPacketHeader::getTotalLen((uint8_t*)pkt->data());
    uint8_t packet[packetLength];
    memset(packet, 0, packetLength);
    memcpy(packet, (uint8_t*)pkt->data(), packetLength);
    pkt->kill();

    /*
        If the packet is from switch and it isn't AID request packet,
        then look at the destination AID and forward the packet to 
        the corresponding port. 
    */
    
	//SL: changed to source and destination addresses instead of AID
	//source and destination addresses have variable length
	HostAddr src = SCIONPacketHeader::getSrcAddr(packet); 
    HostAddr dst = SCIONPacketHeader::getDstAddr(packet);
   
	#ifdef _SL_DEBUG
    //SLT:
    printf("Router(%llu:%llu): RECEIVE type <%d> of size [%d] from Src:%llu, Dst:%llu\n", 
   		m_uAdAid, m_uAid, type, packetLength, src.numAddr(), dst.numAddr());
    #endif
	
	scionPrinter->printLog(IH,type,0,src,dst,"%u,RECEIVED\n",packetLength); 
    
	//SL: change the processing order: most frequent first...
	/*1. Data packet first*/
	if(type == DATA) {//process data packet first...
		if(port) {//data packet from neighbor ADs  
			#ifdef _SL_DEBUG_GW
			printf("Router (%llu:%llu): forward data packet\n", m_uAdAid, m_uAid);
			#endif
			forwardDataPacket(port, packet);
		} else { //data packet from switch; i.e., port == 0
			uint8_t srcLen = SCIONPacketHeader::getSrcLen(packet);
			uint8_t dstLen = SCIONPacketHeader::getDstLen(packet);
			uint8_t pCurrOF = SCIONPacketHeader::getCurrOFPtr(packet);
			uint8_t type = SCIONPacketHeader::getType(packet);
			uint8_t *of = SCIONPacketHeader::getCurrOF(packet);

			if(pCurrOF == (srcLen+dstLen)) {//Skip at the router of the origin AD
			#ifdef _SL_DEBUG_GW
			printf("Router (%llu:%llu): origin -- forward data packet: pCurrOF = %d\n", 
				m_uAdAid, m_uAid, pCurrOF);
			#endif
				forwardDataPacket(port, packet);
			} else {
			#ifdef _SL_DEBUG_GW
			printf("Router (%llu:%llu): write to egress IF: pCurrOF = %d, type=%d\n", 
				m_uAdAid, m_uAid, pCurrOF,type);
			#endif
				if(*of == TDC_XOVR){ //TDC Crossover...
          click_chatter("%s: TDC XOVR", name().c_str());
					SCIONPacketHeader::setTimestampPtr(packet,pCurrOF);
					SCIONPacketHeader::setDownpathFlag(packet);
					SCIONPacketHeader::increaseOFPtr(packet,1);
				} else if (*of == NON_TDC_XOVR){ //Non-TDC Crossover...
					SCIONPacketHeader::setTimestampPtr(packet,pCurrOF);
					SCIONPacketHeader::setDownpathFlag(packet);
					SCIONPacketHeader::increaseOFPtr(packet,2);
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
NSCIONRouter::normalForward(uint8_t type, int port, uint8_t * packet, uint8_t isUppath) {

    uint16_t packetLength = SCIONPacketHeader::getTotalLen(packet);

	if(verifyOF(port, packet)!=SCION_SUCCESS){
		#ifdef _SL_DEBUG_GW
		printf("Router (%llu:%llu): Mac verification failure.\n", m_uAdAid, m_uAid);
		#endif
		scionPrinter->printLog(EH,type,"MAC CHECK FAILED3\n");
		
		return SCION_FAILURE;
	}else{
		//if passed then send to next hop
		int outport=0; //to switch...

		uint8_t * nof = SCIONPacketHeader::getCurrOF(packet);
		opaqueField * pOF = (opaqueField *) nof;

		uint16_t iface;
		
		if(isUppath)
			iface = pOF->ingressIf;
		else
			iface = pOF->egressIf;
		
		//SLT:
		//this part should be removed after having TDC servers set the downpath flag
		if(SCIONPacketHeader::getType(packet)==PATH_REP
			||SCIONPacketHeader::getType(packet)==CERT_REP){
			iface = pOF->egressIf;
		}

		if(!port) { //from switch, then forward to the connected router...
			
			map<uint16_t,int>::iterator itr;
			if((itr = ifid2port.find(iface)) == ifid2port.end()){
				//no port is found... this would not happen
				printf("here iface = %lu\n", iface);
				return SCION_FAILURE;
			}
			outport = itr->second;
			//leaving an AD, hence increase OF pointer.
			SCIONPacketHeader::increaseOFPtr(packet,1);

		} else { //set destination address to that of the egress router (ifid)
			//SL: this is wrong. Src Addr should not be changed by router
			//However, let's think about when this is necessary
			//HostAddr srcAddr(HOST_ADDR_SCION, m_uAid);
			//SCIONPacketHeader::setSrcAddr(packet, srcAddr);
			if(type==CERT_REQ||type==ROT_REQ||type==CERT_REP){
				HostAddr dstAddr = m_servers.find(CertificateServer)->second.addr;  
				SCIONPacketHeader::setDstAddr(packet, dstAddr);
			}else if(iface) { //Non-TDC
				if(type != DATA) {
				std::map<uint16_t,HostAddr>::iterator itr = ifid2addr.find(iface);
				SCIONPacketHeader::setDstAddr(packet, itr->second);
				}
			} else { //TDC
				if(type == PATH_REG || type == PATH_REQ || type == PATH_REP) {
					SCIONPacketHeader::setDstAddr(packet, m_servers.find(PathServer)->second.addr);
				}
			}
		}
		//otherwise, send to switch...
		
		sendPacket(packet, packetLength, outport);
	}	

	return SCION_SUCCESS;
}

int
NSCIONRouter::crossoverForward(uint8_t type, uint8_t info, int port, uint8_t * packet, uint8_t isUppath) {
	//several cases for cross over case
	//1. TDC crossover
	//2. Shortcut
	//3. On-path crossover
	//4. peering link

    uint16_t packetLength = SCIONPacketHeader::getTotalLen(packet);
	//1 TDC crossover

	//MSB of the info field should be 1 (i.e., special opaque field)
	uint8_t pCurrOF;
	//info = info << 1;

	switch(info) {
	case TDC_XOVR:{//if 2nd MSB in the special OF is 0, it's TDC path
		#ifdef _SL_DEBUG_GW
		printf("TDC Crossover reached\n");
		#endif
		//Core up path MAC verification
		if(verifyOF(port, packet)!=SCION_SUCCESS){
			printf("MAC failed 1\n");
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
				SCIONPacketHeader::increaseOFPtr(packet,1);
				//set destination address	
				SCIONPacketHeader::setDstAddr(packet, psAddr);
					
				//sendPacket(packet, packetLength, PORT_TO_SWITCH);

			//1.1.2 other packets are assumed to be not destined to TDC (for now)
			} else {
				#ifdef _SL_DEBUG_GW
				printf("TDC forward a packet to the egress router.\n");
				#endif
				
				//increase one opague field pointer
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
				
				SCIONPacketHeader::increaseOFPtr(packet, 1); //the next OF is the special OF
				//sendPacket(packet, packetLength, PORT_TO_SWITCH); //this should be delivered to the egress IF of TDC
			}
		//1.2 Packet leaves from TDC to down-path
		} else{ //Downpath
			//if passed then set packet header and send to next child
			SCIONPacketHeader::increaseOFPtr(packet,1);
		}
		sendPacket(packet, packetLength, PORT_TO_SWITCH);
		}
		break;

	case NON_TDC_XOVR:{
		#ifdef _SL_DEBUG_GW
		printf("Non-TDC Crossover reached\n");
		#endif
		//MAC verification
		if(verifyOF(port, packet) != SCION_SUCCESS){
			printf("MAC CHECK FAILED @ Crossover AD\n");
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ Crossover AD\n");
			return SCION_FAILURE;
		}
		//2.1 Packet arrive at crossover AD through up-path
		if(isUppath){
			//increase one opague field pointer
			uint16_t ifid;
			// if more than one special OF is present, 
			//the offset should be increased by the number of special OFs -1.
			
			SCIONPacketHeader::increaseOFPtr(packet, 2); //move pointer to the special OF
			sendPacket(packet, packetLength, PORT_TO_SWITCH); //this should be delivered to the egress IF of TDC
		//2.2 Packet leaves from crossover AD to down-path
		} else{ //Downpath
			//the first downpath OF is for the MAC verification
			SCIONPacketHeader::increaseOFPtr(packet,1);
			//if passed then set packet header and send to next child
			SCIONPacketHeader::increaseOFPtr(packet,1);
		}
		sendPacket(packet, packetLength, PORT_TO_SWITCH);
		} 
		break;

	case INPATH_XOVR:{
		//MAC verification
		if(verifyOF(port, packet) != SCION_SUCCESS){
			printf("MAC CHECK FAILED @ On-Path\n");
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ On-Path\n");
			return SCION_FAILURE;
		}
		//a) update TS
		int isRegular = 1;
		int hops = 0;
		while(isRegular) {
			pCurrOF = SCIONPacketHeader::increaseOFPtr(packet,1); 
			isRegular = SCIONPacketHeader::isRegularOF(packet, pCurrOF);
			hops++;
		}

		SCIONPacketHeader::setTimestampPtr(packet, pCurrOF);
		//b) move pointer to the destination AD (i.e., itself) on the downpath
		SCIONPacketHeader::increaseOFPtr(packet,hops); //symmetric on TS 
		//c) another MAC verification for the downpath
		if(verifyOF(port, packet) != SCION_SUCCESS){
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ On-Path Out\n");
			printf("MAC check 4\n");
			return SCION_FAILURE;
		}
		//d) send packet to the destination host
		sendPacket(packet, packetLength, PORT_TO_SWITCH);
		}
		break;

	case INTRATD_PEER:{
		//current OF is added for the previous AD's MAC verification
		//a) move pointer to the peering link OF
		if(isUppath)
			SCIONPacketHeader::increaseOFPtr(packet, 1); 
		//b) MAC verification
		if(verifyOF(port, packet) != SCION_SUCCESS){
			scionPrinter->printLog(EH,type,"MAC CHECK FAILED @ Peering Link Up\n");
			printf("MAC CHECK FAILED @ Peering Link Up\n");
			return SCION_FAILURE;
		}

		//c) move OF pointer by 2 since the next OF is for the current MAC verification
		if(!isUppath)
			SCIONPacketHeader::increaseOFPtr(packet, 2); //move pointer to the special OF

		sendPacket(packet, packetLength, PORT_TO_SWITCH); //this should be delivered to the egress IF of TDC
		}
		break;

	case INTERTD_PEER:
		//SL: Not implemented yet; however this routine would be similar to that of INTRATD_PEER
		break;

	default: 
		printf("Router (%llu:%llu): Unknown crossover type\n", m_uAdAid,m_uAid);
		break;
	}
}


/** 
SL:
General packet forwarding using opaque field
*/
int NSCIONRouter::forwardDataPacket(int port, uint8_t * packet) {
	
	uint8_t pCurrOF = SCIONPacketHeader::getCurrOFPtr(packet); 	//CurrOF*   
	uint8_t *currOF = SCIONPacketHeader::getCurrOF(packet); 	//CurrOF   
	uint8_t oinfo = *currOF;

	uint8_t srcLen = SCIONPacketHeader::getSrcLen(packet);
	uint8_t dstLen = SCIONPacketHeader::getDstLen(packet);
    uint8_t type = SCIONPacketHeader::getType(packet);
	
	#ifdef _SL_DEBUG
	scionCommonHeader * h = (scionCommonHeader *)packet;
	specialOpaqueField * s = (specialOpaqueField *)currOF;
	opaqueField * r = (opaqueField *) currOF;
	printf("Router (%llu:%llu): current OF: %x, pCurrOF = %d, h->currOF=%x, s->info = %x, s->ts = %d., s->hops = %d, info = %x, r->type=%x\n",
		m_uAdAid,m_uAid, currOF, pCurrOF, h->currOF, s->info, s->timestamp, s->hops, oinfo, r->type);
	printf("OF: ");
	for(int ii=0; ii<8; ii++) {
		printf("%02x",*currOF++);
	}
	printf("\n");
	#endif

	oinfo = oinfo >> 7;
	int isRegular = !oinfo; //check if MBS is 0
	//int isRegular = SCIONPacketHeader::isRegularOF(packet, pCurrOF);	//Regular OF: Info field starts w/ 0
	
	#ifdef _SL_DEBUG_OF
	s = (specialOpaqueField *) currOF;
	printf("Router (%llu:%llu): current OF: %x, %d, isRegular= %d, LSB = %d, sizeSOF = %d, sizeOF = %d\n",
	m_uAdAid,m_uAid, currOF, currOF, isRegular, oinfo & 0x01, sizeof(specialOpaqueField), sizeof(opaqueField));
	#endif

	//for the TDC AD edge router
	while(!isRegular) { //special opaque field; i.e., timestamp
		#ifdef _SL_DEBUG_GW
		if(type == DATA)
		printf("Router (%llu:%llu): setting current OF ptr (in reading Timestamp)\n",m_uAdAid,m_uAid);
		#endif
		//1. update TS*
		SCIONPacketHeader::setTimestampPtr(packet, pCurrOF);
        
       	//HACK!!!!!! SJ: TODO
		//2. Set the up-path/down-path flag
		if((type!=PATH_REP && type!=CERT_REP) && pCurrOF == (srcLen + dstLen)){
			SCIONPacketHeader::setUppathFlag(packet);
       	}else{
			if(type == DATA)
			printf("Downpath TS is set\n");
			SCIONPacketHeader::setDownpathFlag(packet);
       	}

		//3. increase the OF pointer to the next one
		pCurrOF = SCIONPacketHeader::increaseOFPtr(packet, 1);
		isRegular = SCIONPacketHeader::isRegularOF(packet, pCurrOF);
		//SL: if a sepecial OF consists of multiple OFs, it should be handled here...
	} 

	//normal opaque field handling for forwarding
	uint8_t pTS = SCIONPacketHeader::getTimestampPtr(packet);	//TS*
	uint8_t info = SCIONPacketHeader::getTimestampInfo(packet, pTS); //Information field in the TS opaque field
	uint8_t of_type = SCIONPacketHeader::getOFType(packet);
	uint32_t TS = SCIONPacketHeader::getTimestamp(packet);	//Timestamp

	//1 bit shift left. 1st bit should be 0 since it's a regular OF
	of_type = of_type << 1; 

	//2nd bit is 1 if another opaque field follows
	//Currently we don't have this case, yet need to process it if needed
	while(of_type & MASK_MSB){ 
		//do some action and increase OF pointer
		pCurrOF = SCIONPacketHeader::increaseOFPtr(packet,1);
		of_type = SCIONPacketHeader::getOFType(packet);
		of_type = of_type << 1; //1 bit shift left, 1st bit should be 0 since it's a regular OF
	}

	of_type = of_type << 1;
	
	//SL: let's extract information from the flag instead of using the flag directly
	//e.g., SCIONPacketHeader::isUpPath(packet)
	uint8_t flag = SCIONPacketHeader::getFlags(packet);
	uint8_t isUppath = flag & MASK_MSB;
	
	//if downpath and peer OF, increase one more OFPtr
	//since the current OF is added for MAC verification
	if(!isUppath && (pCurrOF==pTS+OPAQUE_FIELD_SIZE) && (info&0x10))
	{
	#ifdef _SL_DEBUG_GW
	printf("peering rec. reached\n");
	#endif
		pCurrOF = SCIONPacketHeader::increaseOFPtr(packet, 1);
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
int NSCIONRouter::writeToEgressInterface(int port, uint8_t * packet) {
	uint16_t ifid = SCIONPacketHeader::getOutgoingInterface(packet);
	uint8_t type = SCIONPacketHeader::getType(packet);
    uint16_t packetLength = SCIONPacketHeader::getTotalLen(packet);
	uint8_t pTS = SCIONPacketHeader::getTimestampPtr(packet);	//TS*
	uint8_t info = SCIONPacketHeader::getTimestampInfo(packet, pTS); //Information field in the TS opaque field
	
	//SL: Egress router increase the OF Ptr
	//We have to decide whether the egress router would verify OF again or not.
	//Currently the egress router trusts the ingress router's verification
	SCIONPacketHeader::increaseOFPtr(packet,1);//leaving an AD, hence increase OF pointer.
	if(info &0x10){ //Peer: increase another OF pointer since the next OF is just for verification
		#ifdef _SL_DEBUG_GW
		printf("Crossing peering link\n");
		#endif
		SCIONPacketHeader::increaseOFPtr(packet,1);
	}
	//SLT:
	#ifdef _SL_DEBUG_GW
	printf("Router (%llu:%llu) in writeToEgressInterface: %d: Packet type = %d, length = %d\n",
		m_uAdAid, m_uAid, ifid, type, packetLength);
	#endif

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
int NSCIONRouter::processControlPacket(int port, uint8_t * packet)
{
    uint8_t type = SCIONPacketHeader::getType(packet);
    uint16_t packetLength = SCIONPacketHeader::getTotalLen(packet);

	#ifdef _SL_DEBUG_RT
	//SLT:
	printf("Router (%llu:%llu): in processControlPacket: Packet Length: %d\n",m_uAdAid, m_uAid, packetLength);
	#endif

    //SL: for special control packets that need destination ID to be set to its own AID
	//for logging purpose...
	if(type==AID_REQ||type==IFID_REQ||type==IFID_REP||type==BEACON){
		HostAddr dst = HostAddr(HOST_ADDR_SCION,m_uAdAid);
    }
		
   	/*1 BEACON: Beacon received from the upstream*/
	if(type==BEACON){ //PCB is the most frequent packet except DATA packets
        
		//PCB from Beacon Server.
		if(port == 0) {
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
			//set the destination to the pcb server
			HostAddr bsAddr = m_servers.find(BeaconServer)->second.addr;
			SCIONPacketHeader::setDstAddr(packet, bsAddr);

			//SL: set ingress interface id to let BS know the incoming interface.
			SCIONBeaconLib::setInterface(packet, getIFID(port));
			if(SCIONBeaconLib::getInterface(packet)==0){
				scionPrinter->printLog(EH,type,"Beacon Server ifid not found\n");
				return SCION_FAILURE;
			}

			//sends packet to the beacon server
			sendPacket(packet, packetLength, PORT_TO_SWITCH);
		}
   	
   	}else {
		switch(type) {
		case IFID_REQ:{
			/*2 IFID_REQ: IFID request packet from the neighboring AD edge routers*/
			//IFIDNEW

			#ifdef _SL_DEBUG
			//SLT:
			printf("Router (%llu:%llu): IFID_REQ: Packet Length: %d\n",m_uAdAid, m_uAid,packetLength);
			#endif

			//append the IFID
			uint16_t ifid = port2ifid.find(port)->second;
			//SL: this should be revised... let's don't use pointer directly...
			uint8_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
			*(uint16_t*)(packet+hdrLen) = ifid;
			
			HostAddr dstAddr = SCIONPacketHeader::getSrcAddr(packet);
			//sets the packet header
			//don't need to construct a header; 
			//only need to change the type, src/dst addresses
			SCIONPacketHeader::setType(packet, IFID_REP);
			//SL: this should be handled differently
			//currently only SCION address can be initialized
			HostAddr srcAddr(HOST_ADDR_SCION,m_uAdAid);
			SCIONPacketHeader::setSrcAddr(packet,srcAddr);//set src addr to ADAID (for logging).
			SCIONPacketHeader::setDstAddr(packet,dstAddr); //set dst addr to that of requester (i.e., src)

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
			uint8_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
			uint16_t newIfid = *(uint16_t*)(packet+hdrLen);
			uint16_t ifid = port2ifid.find(port)->second;
			ifid_map.insert(std::pair<uint16_t, uint16_t>(ifid, newIfid));
			//SL: 2nd ifid is the local IFID
			*(uint16_t*)(packet+hdrLen+IFID_SIZE) = ifid;

			//SLT: need to consider multiple BSs later...
			HostAddr bsAddr = m_servers.find(BeaconServer)->second.addr;
			//note: addDstAddr would either insert a dest addr or replace the current one with the new one.
			HostAddr srcAddr(HOST_ADDR_SCION, m_uAid);
			SCIONPacketHeader::setSrcAddr(packet, srcAddr);
			SCIONPacketHeader::setDstAddr(packet, bsAddr);
		
			//sends to beacon server
			sendPacket(packet, packetLength, 0);
			}
			break;

		case AID_REQ: {
    		/*4 AID_REQ: AID Request from the switch*/
			SCIONPacketHeader::setType(packet, AID_REP);
			HostAddr addr(HOST_ADDR_SCION,m_uAid);
			SCIONPacketHeader::setSrcAddr(packet, addr);

			sendPacket(packet, packetLength, 0);
			}
			break;

		default:
			/*5 Forward all other control packets from neighbor AD using opaque field*/
			//in general, this should not be reached...
			if(forwardDataPacket(port, packet)) {
				//if this part is reached, something wrong happend.
				printf("Something wrong happened%llu -- not handled control packet\n",m_uAdAid);
				return SCION_FAILURE;
			}
			break;
		}

    }
}

/*
    NSCIONRouter::getIFID
    get IFID that represents this click port number
*/
uint16_t NSCIONRouter::getIFID(int port){
    std::map<int, uint16_t>::iterator itr;
    if((itr=port2ifid.find(port))==port2ifid.end()){
        return 0;
    }
    return itr->second;
}


bool 
NSCIONRouter::getOfgKey(uint32_t timestamp, aes_context &actx){

	if(timestamp > m_currOfgKey.time) {
		actx = m_currOfgKey.actx;
	} else {
		actx = m_prevOfgKey.actx;
	}
    return SCION_SUCCESS;

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
}

/*
SL:
Check if interface ID of an opaque field matches with the current incoming interface
dir =1: uppath, dir = 0: downpath
*/
int NSCIONRouter::verifyInterface(int port, uint8_t * packet, uint8_t dir) {

	uint16_t in_ifid, e_ifid, ifid;
	uint8_t type = SCIONPacketHeader::getType(packet);
    in_ifid = SCIONPacketHeader::getIngressInterface(packet);
	e_ifid = SCIONPacketHeader::getEgressInterface(packet);
	if((dir && e_ifid) || (!in_ifid && !dir)) //either on uppath (except the source AD) or on downpath at TDC
		ifid = e_ifid;
	else
		ifid = in_ifid;
		
	int inPort = 0;

	map<int, uint16_t>::iterator itr;
	if((itr = port2ifid.find(port)) == port2ifid.end()) {
		#ifdef _SL_DEBUG_GW
		printf("in_ifid= %d, e_ifid= %d: Interface is not found in port %d.\n",in_ifid, e_ifid, port);
		#endif

		scionPrinter->printLog(EH,type,"Interface verification failure: interface not found\n");
		return SCION_FAILURE;
	}

	
	if(itr->second==ifid) {
		return SCION_SUCCESS;
	} else {
		#ifdef _SL_DEBUG_GW
		printf("IFID mismatch: info=%x: in_ifid= %d, e_ifid= %d: Input port ifid:%d ifid-on-port: %d.\n", 
			*(uint8_t*)packet, in_ifid, e_ifid, ifid, itr->second);
		#endif
		scionPrinter->printLog(EH,type,"Interface verification failure: input port mismatch\n");
		return SCION_FAILURE;
	}
}

/****************************************************
    NSCIONRouter:: MAC VERIFICATION FUNCTIONS
*****************************************************/
//SL:
//All verification is performed by this function
int NSCIONRouter::verifyOF(int port, uint8_t * packet) {
	
	uint8_t flag = SCIONPacketHeader::getFlags(packet);
	uint8_t isUppath = flag & MASK_MSB;
    uint32_t timestamp = SCIONPacketHeader::getTimestamp(packet);

	if(port){ //if packet is coming from an external AD 
	// port 0 is assigned to connect switch (i.e., internal)
		if(verifyInterface(port, packet, isUppath) == SCION_FAILURE){
		    printf("interface verification failed\n");
        	return SCION_FAILURE;
        }
	}

    aes_context actx;
	getOfgKey(timestamp,actx);
	uint8_t * nof = SCIONPacketHeader::getCurrOF(packet);
	uint8_t * ncof = SCIONPacketHeader::getOF(packet,1);
	opaqueField * of = (opaqueField *)nof;
	opaqueField * chained_of = (opaqueField *) ncof;
	opaqueField no = opaqueField();

	#ifdef _SL_DEBUG_OF
	printf("Router (%llu:%llu): Verifying MAC>> of->ingressIf: %d, of->egressIf: %d, c->ingressIf = %d, c->egressIf = %d\n"
		,m_uAdAid, m_uAid, of->ingressIf, of->egressIf, chained_of->ingressIf, chained_of->egressIf);
	#endif

	//SL: getNextOF & getPreviousOF need to be implemented.
	//Use ::getOF(uint8_t * pkt, int offset)
	//offset = 1 => next OF, offset = -1 => previous OF
	if(isUppath){
		if(of->ingressIf) {//Non-TDC
    		ncof = SCIONPacketHeader::getOF(packet,1);
    		chained_of = (opaqueField *) ncof;
		}else { //TDC
			chained_of = &no; //i.e., set to 0
		}
	//2. Down-path
    }else{
		if(of->ingressIf){ //Non-TDC
    		ncof = SCIONPacketHeader::getOF(packet,-1);
    		chained_of = (opaqueField *) ncof;
		} else {//TDC
			chained_of = &no; //i.e., set to 0
		}
    }
    
	#ifdef _SL_DEBUG
	printf("Router (%llu:%llu): Verifying MAC: in: %d, e: %d, C_in: %d, C_e: %d.\n"
		,m_uAdAid, m_uAid, of->ingressIf, of->egressIf, chained_of->ingressIf, chained_of->egressIf);
	#endif
	uint8_t exp = 0x03 & of->type;
	return SCIONBeaconLib::verifyMAC(timestamp, exp, of->ingressIf, of->egressIf,
    *(uint64_t *)&chained_of,of->mac,&actx);
}

int NSCIONRouter::verifyUpPacket(uint8_t* pkt, uint32_t timestamp){
	return SCION_SUCCESS;
}

int NSCIONRouter::verifyUpPacketCore(uint8_t* pkt, uint32_t timestamp){
   	return SCION_SUCCESS;
}

int NSCIONRouter::verifyDownPacket(uint8_t* pkt, uint32_t timestamp){
   	return SCION_SUCCESS;
}

int NSCIONRouter::verifyDownPacketCore(uint8_t* pkt, uint32_t timestamp){
   	return SCION_SUCCESS; 
}


int NSCIONRouter::verifyDownPacketEnd(uint8_t* pkt, uint32_t timestamp){
   	return SCION_SUCCESS;
}

int NSCIONRouter::verifyUpPeer(uint8_t* pkt, uint32_t timestamp){
   	return SCION_SUCCESS;
}

int NSCIONRouter::verifyDownPeer(uint8_t* pkt, uint32_t timestamp){
   	return SCION_SUCCESS;
}

int NSCIONRouter::verifyDownPeerEnd(uint8_t* pkt, uint32_t timestamp){
   	return SCION_SUCCESS;
}


/*
    NSCIONRouter::sendPacket
    -creates click packet using the given data and send it out to the given port
*/
void NSCIONRouter::sendPacket(uint8_t* data, uint16_t data_length, int port){
    uint8_t type = SCIONPacketHeader::getType(data);
        
    HostAddr src = SCIONPacketHeader::getSrcAddr(data); 
    HostAddr dst = SCIONPacketHeader::getDstAddr(data);
    
	//SLA:
	//IPv4 handling here
	//if addrtype == IPv4, encap with IP header
	//otherwise
	scionPrinter->printLog(IH,type,0,src,dst,"%u,SENT\n",data_length); 
    WritablePacket *outPacket = Packet::make(DEFAULT_HD_ROOM, data, data_length,
        DEFAULT_TL_ROOM);
    output(port).push(outPacket);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(NSCIONRouter)


