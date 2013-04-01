/*****************************************
 * File Name : packetheader.cc

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 28-03-2012

 * Purpose : 

******************************************/

#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<click/config.h>
#include"packetheader.hh"

CLICK_DECLS

void SCIONPacketHeader::initPacket(uint8_t* pkt, uint16_t type, uint16_t totalLength, uint64_t
        src, uint64_t dst, uint32_t upTs, uint32_t dwTs, uint8_t hops, uint16_t interface){
}

/*returns the current type of the packet*/
uint8_t SCIONPacketHeader::getType(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->type;
}
/*returns the header length of the packet*/
uint8_t SCIONPacketHeader::getHdrLen(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->hdrLen;
}
/*returns the total length of the packet*/
uint16_t SCIONPacketHeader::getTotalLen(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->totalLen;
}
/*returns the current timestamp(32 bit)*/
uint32_t SCIONPacketHeader::getTimestamp(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    uint8_t tsOffset = h->timestamp;
    uint32_t ts = *(uint32_t*)(pkt+COMMON_HEADER_SIZE+tsOffset+1);
    return ts;
}
/*set the current timestamp opaque field*/
void SCIONPacketHeader::setTimestampOF(uint8_t* pkt, specialOpaqueField& ts){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
	uint8_t hdrLen = getHdrLen(pkt);
    h->timestamp = hdrLen-COMMON_HEADER_SIZE;
	memcpy(pkt+hdrLen, &ts, OPAQUE_FIELD_SIZE); 
}
uint8_t SCIONPacketHeader::getTimestampPtr(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->timestamp;
}


uint8_t SCIONPacketHeader::getTimestampInfo(uint8_t* pkt, uint8_t pTS){
    specialOpaqueField* sOFptr =
        (specialOpaqueField*)(pkt+pTS+COMMON_HEADER_SIZE);
    return sOFptr->info; 
}

/*returns the source address length*/
uint8_t SCIONPacketHeader::getSrcLen(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->srcLen;
}
/*returns the destination address length*/
uint8_t SCIONPacketHeader::getDstLen(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->dstLen;
}
/*returns the 8 bits flags*/
uint8_t SCIONPacketHeader::getFlags(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->flag;
}
/*returns the 8 byte opaque field*/
uint8_t* SCIONPacketHeader::getCurrOF(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    uint8_t * currOF = (uint8_t*)(pkt+COMMON_HEADER_SIZE+h->currOF);
	#ifdef _SL_DEBUG_PH
	printf("in getCurrOF: h->currOF = %d\n", h->currOF);
	#endif
    return currOF;
}

/*returns the opaque field deplaced by the offset*/
uint8_t* SCIONPacketHeader::getOF(uint8_t* pkt, int offset){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    uint8_t currOfOffset = h->currOF;
    uint8_t* currOF = (uint8_t*)(pkt+COMMON_HEADER_SIZE+currOfOffset+offset*OPAQUE_FIELD_SIZE);
	#ifdef _SL_DEBUG_PH
	printf("in getOF: currOfOffset = %d, offset=%d\n", currOfOffset, offset);
	#endif
    return currOF;
}


/*SL: returns the 8 byte opaque field*/
uint8_t SCIONPacketHeader::getOFPtr(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->currOF;
}

uint8_t SCIONPacketHeader::getOFType(uint8_t* pkt){
    uint8_t * of = getCurrOF(pkt);
	opaqueField * pOF = (opaqueField *) getCurrOF(pkt);
    return pOF->type;
}

/*returns the number of the opaque field*/
uint8_t SCIONPacketHeader::getNumOF(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->numOF;
}
/*returns the type of layer 4 protocol*/
uint8_t SCIONPacketHeader::getL4Proto(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->l4Proto;
}
/*returns the number of return capabilities*/
uint8_t SCIONPacketHeader::getNRetCap(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->nRetCap;
}
/*reutrns the capability req info*/
uint8_t SCIONPacketHeader::getCapReqInfo(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->capReqInfo;
}
/*returns the offset to the new capability*/
uint8_t SCIONPacketHeader::getNewCap(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->nRetCap+COMMON_HEADER_SIZE;
}
/*returns the offset to the path validation*/
uint8_t SCIONPacketHeader::getPathVal(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->pathVal+COMMON_HEADER_SIZE;
}
/*returns the offset to the sourceh authentication*/
uint8_t SCIONPacketHeader::getSrcAuth(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->srcAuth+COMMON_HEADER_SIZE;
}

/* SLT: this has not been implemented anywhere...
HostAddr SCIONPacketHeader::getSrcAddr(uint8_t* pkt){
    uint8_t srcLen = getSrcLen(pkt);
    uint8_t type = *(uint8_t*)(pkt+COMMON_HEADER_SIZE);
    HostAddr srcAddr = HostAddr();
    srcAddr.setAddrType(type);
//    *srcAddr = (uint8_t*)malloc(srcLen);
    srcAddr.setAddr(type,pkt+COMMON_HEADER_SIZE);
	return srcAddr;
}

HostAddr SCIONPacketHeader::getDstAddr(uint8_t* pkt){
    uint8_t srcLen = getSrcLen(pkt);
    uint8_t dstLen = getDstLen(pkt);
    uint8_t type = *(uint8_t*)(pkt+COMMON_HEADER_SIZE+srcLen);
    HostAddr dstAddr = HostAddr();
    dstAddr.setAddrType(type);
//    *srcAddr = (uint8_t*)malloc(srcLen);
    dstAddr.setAddr(type,pkt+COMMON_HEADER_SIZE+srcLen);
	return dstAddr;
}
*/

void SCIONPacketHeader::getSrcAddr(uint8_t* pkt, uint8_t** srcAddr){
    uint8_t srcLen = getSrcLen(pkt);
    *srcAddr = (uint8_t*)malloc(srcLen);
    memcpy(*srcAddr, pkt+COMMON_HEADER_SIZE, srcLen);
}


HostAddr SCIONPacketHeader::getSrcAddr(uint8_t* pkt){
    uint8_t srcLen = getSrcLen(pkt);
	HostAddr addr;
	
	uint8_t offset = COMMON_HEADER_SIZE;
	
	switch(srcLen) {
	case SCION_ADDR_SIZE:
		addr.setSCIONAddr(*(uint64_t *)(pkt+offset));
	break;
	case IPV4_SIZE:
		addr.setIPv4Addr(*(uint32_t*)(pkt+offset));
	break;
	case IPV6_SIZE:
		addr.setIPv6Addr(pkt+offset);
	break;
	case AIP_SIZE:
		addr.setAIPAddr(pkt+offset);
	break;
	default: break;
	}
	return addr;
}

HostAddr SCIONPacketHeader::getDstAddr(uint8_t* pkt){
    uint8_t srcLen = getSrcLen(pkt);
    uint8_t dstLen = getDstLen(pkt);
	HostAddr addr;

	uint8_t offset = COMMON_HEADER_SIZE+srcLen;

	switch(dstLen) {
	case SCION_ADDR_SIZE:
		addr.setSCIONAddr(*(uint64_t*)(pkt+offset));
	break;
	case IPV4_SIZE:
		addr.setIPv4Addr(*(uint64_t*)(pkt+offset));
	break;
	case IPV6_SIZE:
		addr.setIPv6Addr(pkt+offset);
	break;
	case AIP_SIZE:
		addr.setAIPAddr(pkt+offset);
	break;
	default: break;
	}
	return addr;
}


void SCIONPacketHeader::getDstAddr(uint8_t* pkt, uint8_t** dstAddr){
    uint8_t srcLen = getSrcLen(pkt);
    uint8_t dstLen = getDstLen(pkt);
    *dstAddr = (uint8_t*)malloc(dstLen);
    memcpy(*dstAddr, pkt+COMMON_HEADER_SIZE+srcLen, dstLen);
}

void SCIONPacketHeader::setType(uint8_t* pkt, uint8_t type){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->type = type;
}
void SCIONPacketHeader::setHdrLen(uint8_t* pkt, uint8_t hdrLen){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->hdrLen = hdrLen;
}
//SLT:
//change header length and total length by the given amount
void SCIONPacketHeader::adjustPacketLen(uint8_t* pkt, int offset){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->hdrLen += offset;
	h->totalLen += offset;
}

void SCIONPacketHeader::setTotalLen(uint8_t* pkt, uint16_t totalLen){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->totalLen = totalLen;
}
void SCIONPacketHeader::setTimestampPtr(uint8_t* pkt, uint8_t tsPtr){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->timestamp = tsPtr;
}
void SCIONPacketHeader::setSrcLen(uint8_t* pkt, uint8_t srcLen){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->srcLen = srcLen;
}
void SCIONPacketHeader::setDstLen(uint8_t* pkt, uint8_t dstLen){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->dstLen = dstLen;
}
void SCIONPacketHeader::setFlags(uint8_t* pkt, uint8_t flag){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->flag = flag;
}
void SCIONPacketHeader::setCurrOFPtr(uint8_t* pkt, uint8_t currOFPtr){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->currOF = currOFPtr;
}
void SCIONPacketHeader::setNumOF(uint8_t* pkt, uint8_t numOF){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->numOF = numOF;
}
void SCIONPacketHeader::setL4Proto(uint8_t* pkt, uint8_t l4Proto){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->l4Proto = l4Proto;
}
void SCIONPacketHeader::setNRetCap(uint8_t* pkt, uint8_t nRetCap){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->nRetCap = nRetCap;
}
void SCIONPacketHeader::setCapReqInfo(uint8_t* pkt, uint8_t capReqInfo){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->capReqInfo = capReqInfo;
}
void SCIONPacketHeader::setNewCapPtr(uint8_t* pkt, uint8_t newCap){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->newCap = newCap;
}
void SCIONPacketHeader::setPathValPtr(uint8_t* pkt, uint8_t pathVal){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->pathVal = pathVal;
}
void SCIONPacketHeader::setSrcAuthPtr(uint8_t* pkt, uint8_t srcAuth){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->srcAuth = srcAuth;
}




void SCIONPacketHeader::addTotalLen(uint8_t* pkt, uint16_t len){
    scionCommonHeader* h= (scionCommonHeader*)pkt;
    h->totalLen+=len;
}
void SCIONPacketHeader::addNumOF(uint8_t* pkt, uint8_t numOF){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    h->numOF+=numOF;
}
/*
    Sets the source address of the packet, and sets the srcLen field to the given
    srcLen.
*/
void SCIONPacketHeader::setSrcAddr(uint8_t* pkt, HostAddr srcAddr){
    uint8_t addrLen = srcAddr.getLength();
	if(!addrLen)
		return;

    uint8_t addr[addrLen];
    uint8_t addrType = srcAddr.getType();
    switch(addrType){
        case HOST_ADDR_SCION:srcAddr.getSCIONAddr(addr); break;
        case HOST_ADDR_IPV4:srcAddr.getIPv4Addr(addr); break;
        case HOST_ADDR_IPV6:srcAddr.getIPv6Addr(addr); break;
        case HOST_ADDR_AIP:srcAddr.getAIPAddr(addr); break;
        default: break;
    }
    setSrcLen(pkt,addrLen);
    memcpy(pkt+COMMON_HEADER_SIZE,addr,addrLen);
}
/*
    Sets the destination address of the packet, and sets the dstLen field to the
    given dstLen.
*/
void SCIONPacketHeader::setDstAddr(uint8_t* pkt, HostAddr dstAddr){
    uint8_t addrLen = dstAddr.getLength();
	if(!addrLen)
		return;

    uint8_t addr[addrLen];
    uint8_t addrType = dstAddr.getType();
    switch(addrType){
        case HOST_ADDR_SCION:dstAddr.getSCIONAddr(addr); break;
        case HOST_ADDR_IPV4:dstAddr.getIPv4Addr(addr); break;
        case HOST_ADDR_IPV6:dstAddr.getIPv6Addr(addr); break;
        case HOST_ADDR_AIP:dstAddr.getAIPAddr(addr); break;
        default: break;
    }
    setDstLen(pkt,addrLen);
	uint8_t srcLen = getSrcLen(pkt);
    if(addrLen==0){
//        printf("Warning: source address is not set. This may cause issues.\n");
//        printf("    - Please make sure you set source address BEFORE you set \n");
//        printf("      destination address\n");
    }
    memcpy(pkt+COMMON_HEADER_SIZE+srcLen,addr,addrLen);
}

/*
	SL:
    Add destination address to the packet
	If a destination address already exists in the packet,
	it is replaced by the given address
*/
void SCIONPacketHeader::addDstAddr(uint8_t* pkt, HostAddr dstAddr){
	uint16_t packetLen = getTotalLen(pkt);
	uint8_t srcLen = getSrcLen(pkt);
    uint8_t dstLen = dstAddr.getLength();
    uint8_t addr[dstLen];
    uint8_t addrType = dstAddr.getType();
	uint8_t pkt_dst_len = getDstLen(pkt);
	uint8_t buf[packetLen+dstLen-pkt_dst_len];

	//make room for the destination address
	memset(buf, 0, packetLen+dstLen-pkt_dst_len);
	
	#ifdef _SL_DEBUG
	printf("\t in addDstAddr: packetLen:%d, dstLen:%d, pkt_dst_len:%d, srcLen:%d\n",packetLen, dstLen,pkt_dst_len,srcLen);
	#endif

	//1. copy the packet content up to src address
	memcpy(buf, pkt, COMMON_HEADER_SIZE+srcLen);
	
	//2. copy the remaining content (i.e., payload) after the destination address
	memcpy(&buf[COMMON_HEADER_SIZE+srcLen+dstLen], pkt+COMMON_HEADER_SIZE+srcLen+pkt_dst_len, 
		packetLen-COMMON_HEADER_SIZE-srcLen-pkt_dst_len);
    
	//3. not squeeze in the destination address between the src address and the payload
	setDstAddr(buf, dstAddr);
	setDstLen(buf, dstAddr.getLength());
	setTotalLen(buf, packetLen+dstLen-pkt_dst_len);
    memcpy(pkt, buf, packetLen+dstLen-pkt_dst_len);
}

/*
	SLT:
    Add source address to the packet
	If a source address already exists in the packet,
	it is replaced by the given address
*/
void SCIONPacketHeader::addSrcAddr(uint8_t* pkt, HostAddr srcAddr){
	uint16_t packetLen = getTotalLen(pkt);
	uint8_t pkt_src_len = getSrcLen(pkt);
    uint8_t srcLen = srcAddr.getLength();
    uint8_t addr[srcLen];
    uint8_t addrType = srcAddr.getType();
	uint8_t buf[packetLen+srcLen-pkt_src_len];

	//make room for the destination address
	memset(buf, 0, packetLen+srcLen-pkt_src_len);
	//1. copy the packet content up to src address
	memcpy(buf, pkt, COMMON_HEADER_SIZE);
	
	//2. copy the remaining content (i.e., payload) after the destination address
	memcpy(&buf[COMMON_HEADER_SIZE+srcLen], pkt+COMMON_HEADER_SIZE+pkt_src_len, 
		packetLen-COMMON_HEADER_SIZE-pkt_src_len);
    
	//3. not squeeze in the destination address between the src address and the payload
	setSrcAddr(buf, srcAddr);
	setSrcLen(buf, srcAddr.getLength());
	setTotalLen(buf, packetLen+srcLen-pkt_src_len);
    memcpy(pkt, buf, packetLen+srcLen-pkt_src_len);
}

/*
	SLT:
    Remove source address and space from the packet
*/
void SCIONPacketHeader::removeSrcAddr(uint8_t* pkt){
	uint16_t packetLen = getTotalLen(pkt);
	uint8_t pkt_src_len = getSrcLen(pkt);
	uint8_t buf[packetLen-pkt_src_len];

	//1. copy the common header 
	memcpy(buf, pkt, COMMON_HEADER_SIZE);
	
	//2. copy the remaining content (i.e., payload) from the destination address
	memcpy(&buf[COMMON_HEADER_SIZE], pkt+COMMON_HEADER_SIZE+pkt_src_len, 
		packetLen-COMMON_HEADER_SIZE-pkt_src_len);
    
	setSrcLen(buf, 0);
	setTotalLen(buf, packetLen-pkt_src_len);
    memcpy(pkt, buf, packetLen-pkt_src_len);
}

/*
	SLT:
    Remove destination address and space from the packet
*/
void SCIONPacketHeader::removeDstAddr(uint8_t* pkt){
	uint16_t packetLen = getTotalLen(pkt);
	uint8_t pkt_src_len = getSrcLen(pkt);
	uint8_t pkt_dst_len = getDstLen(pkt);
	uint8_t buf[packetLen-pkt_dst_len];

	//1. copy the common header + src addr 
	memcpy(buf, pkt, COMMON_HEADER_SIZE+pkt_src_len);
	
	//2. copy the remaining content (i.e., payload) from the destination address
	memcpy(buf+COMMON_HEADER_SIZE+pkt_src_len, pkt+COMMON_HEADER_SIZE+pkt_src_len+pkt_dst_len, 
		packetLen-COMMON_HEADER_SIZE-pkt_src_len-pkt_dst_len);
    
	setDstLen(buf, 0);
	setTotalLen(buf, packetLen-pkt_dst_len);
    memcpy(pkt, buf, packetLen-pkt_dst_len);
}

/*
	SLT:
    Clear source address while keeping the current length (i.e., addr type).
*/
void SCIONPacketHeader::clearSrcAddr(uint8_t* pkt){
    memcpy(pkt+COMMON_HEADER_SIZE, 0, getSrcLen(pkt));
}

/*
	SLT:
    Clear destination address while keeping the current length (i.e., addr type).
*/
void SCIONPacketHeader::clearDstAddr(uint8_t* pkt){
    memcpy(pkt+COMMON_HEADER_SIZE+getSrcLen(pkt), 0, getDstLen(pkt));
}


/*
    Add the opaque fields for the data forwarding.
    Sets curr OF ptr
    Sets NumOF
    Sets timestamp ptr 
*/
void SCIONPacketHeader::setPath(uint8_t* pkt, uint8_t* path, uint8_t pathLen,
uint8_t numOF){
    uint8_t pathOffset = getSrcLen(pkt)+getDstLen(pkt);
    uint8_t* pathPtr = pkt+pathOffset+COMMON_HEADER_SIZE;
    memcpy(pathPtr, path, pathLen);

    setCurrOFPtr(pkt,pathOffset);
    setNumOF(pkt, numOF);
    setTimestampPtr(pkt, pathOffset+1);
}


/*
    this function puts special opaque field into the packet
*/
void SCIONPacketHeader::putSpecialOpaque(uint8_t* pkt, uint8_t info, uint32_t timestamp, uint16_t
tdid, uint8_t numHop, uint8_t offset){
    *(uint8_t*)(pkt+offset) = info;
    *(uint32_t*)(pkt+offset+1) = timestamp;
    *(uint16_t*)(pkt+offset+5) = tdid;
    *(uint8_t*)(pkt+offset+7) = numHop;
    addTotalLen(pkt, 8);
}

/* SL: check if the current OF is a regular OF */
uint8_t SCIONPacketHeader::isRegularOF(uint8_t * pkt, uint8_t pCurrOF) {
    //scionCommonHeader* h = (scionCommonHeader*)pkt;
    //uint8_t currOfOffset = h->currOF;
    uint8_t * currOF = (uint8_t*)(pkt+COMMON_HEADER_SIZE+pCurrOF);
	return !(*currOF & 0x80);
}

/* SL: set uppath flag in the scionCommonHeader */
void SCIONPacketHeader::setUppathFlag(uint8_t * pkt) {
	uint8_t upflag = 0x80;
    scionCommonHeader* h = (scionCommonHeader*)pkt;
	h->flag |= upflag;
}

/* SL: set downpath flag in the scionCommonHeader */
void SCIONPacketHeader::setDownpathFlag(uint8_t * pkt) {
	uint8_t downflag = ~(0x80);
    scionCommonHeader* h = (scionCommonHeader*)pkt;
	h->flag &= downflag;
}

/* SL: increase OF pointer by cnt OFs */
uint8_t SCIONPacketHeader::increaseOFPtr(uint8_t * pkt, uint8_t cnt) {
    scionCommonHeader* h = (scionCommonHeader*)pkt;
	h->currOF += cnt * OPAQUE_FIELD_SIZE;
	return h->currOF;
}

/*SL: return the outgoing interface id (differ whether a packet is on the uppath or on the downpath) to which a data packet is forwarded
This function is used for handling data packets and other inter-AD control packets
*/
uint16_t SCIONPacketHeader::getOutgoingInterface(uint8_t* pkt) {
	
	uint8_t srcLen = SCIONPacketHeader::getSrcLen(pkt);
	uint8_t dstLen = SCIONPacketHeader::getDstLen(pkt);
	uint8_t offset = srcLen + dstLen;
	uint8_t type = SCIONPacketHeader::getType(pkt);

	opaqueField * of = (opaqueField *) SCIONPacketHeader::getCurrOF(pkt);
	specialOpaqueField * sof = (specialOpaqueField *) SCIONPacketHeader::getCurrOF(pkt);
	//SL: the first OF of DATA (i.e., Timestamp OF) should be skipped...
	if((type == DATA) && (SCIONPacketHeader::getCurrOFPtr(pkt) == offset)) {
		//This OF should be regular...
		of = (opaqueField *) SCIONPacketHeader::getOF(pkt,1);
	}

    int isRegular = !(of->type&0x80);
	uint8_t flag = SCIONPacketHeader::getFlags(pkt);
	uint8_t isUppath = flag & 0x80;

	#ifdef _SL_DEBUG
	if(type == DATA)
	printf("Out-IF: isRegular = %d, isUppath = %d, of->type = %d, sof->info = %d, ingressIF = %d, egressIF = %d\n", 
		isRegular, isUppath, of->type, sof->info, of->ingressIf, of->egressIf);
	#endif
	//SL: note -- this function is called by a switch to handle data packets 
	//and switches in crossover ADs always see a special OF 

	if(isRegular) {
		if(isUppath)
			return of->ingressIf;
		else
			return of->egressIf;
	} else { //i.e., crossover
		uint8_t pTS = SCIONPacketHeader::getTimestampPtr(pkt);	//TS*
		uint8_t info = SCIONPacketHeader::getTimestampInfo(pkt, pTS); //Information field in the TS opaque field
		uint8_t isPeer = info & 0x10;
	
		info = info <<1;
		
		//1. TDC: read the egress IF of the next OF
		if(!(info & 0x80)) {
			of = (opaqueField*) SCIONPacketHeader::getOF(pkt,1);	
			return of->egressIf;
		//2. peering link: read the egress IF of the previous to the previous OF
		} else if(isPeer) {
			of = (opaqueField *) SCIONPacketHeader::getOF(pkt,0);
			return of->ingressIf;
		//3. shortcut or onpath: read the egress IF of the next to the next OF
		} else {
			of = (opaqueField*) SCIONPacketHeader::getOF(pkt,2);
			return of->egressIf;
		}
	}

	
}

/*SL: return the ingress interface id in the PCB (i.e., interface to a provider AD)
*/
uint16_t SCIONPacketHeader::getIngressInterface(uint8_t* pkt) {
	uint8_t * of = SCIONPacketHeader::getCurrOF(pkt);
	opaqueField * pOF = (opaqueField *) of;

	return pOF->ingressIf;
}


/*SL: return the egress interface id in the PCB (i.e., interface to a customer AD)
*/
uint16_t SCIONPacketHeader::getEgressInterface(uint8_t* pkt) {
	uint8_t * of = SCIONPacketHeader::getCurrOF(pkt);
	opaqueField * pOF = (opaqueField *) of;

	return pOF->egressIf;
}


/*SL: returns the current OF pointer*/
uint8_t SCIONPacketHeader::getCurrOFPtr(uint8_t* pkt){
    scionCommonHeader* h = (scionCommonHeader*)pkt;
    return h->currOF;
}

/*SL:
	set scion header with scionHeader
	This simplifies scion packet construction
*/
uint8_t
SCIONPacketHeader::setHeader(uint8_t * packet, scionHeader & hdr) {
	memcpy(packet, &hdr.cmn, sizeof(scionCommonHeader));
	setSrcAddr(packet, hdr.src);
	setDstAddr(packet, hdr.dst);

	uint8_t offset = COMMON_HEADER_SIZE + hdr.src.getLength() + hdr.dst.getLength();

	if(hdr.n_of) {
		uint8_t len = hdr.n_of * OPAQUE_FIELD_SIZE;
		memcpy(packet+offset, hdr.p_of, len);
		setTimestampPtr(packet,offset);
		offset += len;
	}

	setHdrLen(packet,offset);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(SCIONPacketHeader)


