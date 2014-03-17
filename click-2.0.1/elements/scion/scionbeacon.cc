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

#include <polarssl/rsa.h>
#include <polarssl/x509.h>
#include <polarssl/aes.h>
#include <click/config.h>
#include <click/packet.hh>
#include <time.h>
#include "scionbeacon.hh" 
#include "scioncryptolib.hh"
#define DEBUG

CLICK_DECLS


/*
    SCIONBeaconLib::createMAC
    - generates MAC with the given ingress,egress,old mac, using the provided key
*/
uint32_t SCIONBeaconLib::createMAC(uint32_t ts, uint8_t exp, uint16_t ingress, uint16_t egress, uint64_t prev_of, aes_context* ctx)
{
    //uint8_t msgSize = MAC_TS_SIZE + EXP_SIZE + sizeof(uint16_t)*2+sizeof(uint64_t);//TS+EXP+Interfaces+previous OF
    uint8_t msgSize = 16;
    uint8_t msg[msgSize];
    memset(msg, 0 , msgSize);
	//SL: This only works for a little endian
	memcpy(msg, &ts, MAC_TS_SIZE);
    uint8_t* ex = (uint8_t*)(msg+MAC_TS_SIZE);
    *ex=exp;
    uint16_t* inif = (uint16_t*)(msg+MAC_TS_SIZE+EXP_SIZE);
    *inif=ingress;
    uint16_t* egif = (uint16_t*)(msg+MAC_TS_SIZE+EXP_SIZE+sizeof(uint16_t));
    *egif = egress;
    uint64_t* om = (uint64_t*)(msg+MAC_TS_SIZE+EXP_SIZE+sizeof(uint16_t)*2);
    *om = prev_of;
    return (0x00ffffff & SCIONCryptoLib::genMAC(msg, msgSize, ctx));
}


/*
    SCIONBeaconLib::verifyMAC
    - check if the given MAC is valid by comparing the target MAC with the newly
      generated MAC.
*/
int SCIONBeaconLib::verifyMAC(uint32_t ts, uint8_t exp, uint16_t ingress, uint16_t egress, uint64_t prev_of, uint32_t mac, aes_context* ctx)
{
    uint32_t target = createMAC(ts, exp, ingress, egress, prev_of, ctx);

	target = target & 0x00ffffff;
    if(target==mac){
        return SCION_SUCCESS;
    }else{
        return SCION_FAILURE;
    }
}


/*
    SCIONBeaconLib::addLink
    - adds a link ( a pcb marking ) to the beacon. 
*/
uint8_t SCIONBeaconLib::addLink(uint8_t* pkt,uint16_t ingress,uint16_t egress,
    uint8_t type, uint64_t aid, uint32_t tdid, aes_context* ctx, uint8_t ofType,
    uint8_t exp,uint16_t bwAlloc, uint16_t sigLen)
{

    if(!type && egress!=0){
        return SCION_FAILURE;
    }
    
    uint16_t totalLen = SPH::getTotalLen(pkt);
	uint32_t ts = SPH::getTimestamp(pkt);
    uint8_t hops = getNumHops(pkt);
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    uint8_t* mrkItr = (uint8_t*)(pkt+hdrLen+OPAQUE_FIELD_SIZE*2); 
    pcbMarking* mrkPtr = (pcbMarking*)mrkItr;
    
    for(int i=0;i<hops-1;i++){
        mrkItr += (mrkPtr->blkSize + mrkPtr->sigLen);
        mrkPtr = (pcbMarking*)(mrkItr);
    }
    
	uint64_t prev_of;
	opaqueField of= opaqueField(mrkPtr->type, mrkPtr->ingressIf, mrkPtr->egressIf, mrkPtr->mac,0);
    
    if(type==PCB_TYPE_CORE)
        prev_of = 0;
    else 
		prev_of = *(uint64_t *)&of;

	uint32_t mac = createMAC(ts, exp, ingress, egress, prev_of, ctx);

	//SLT:
	//This part needs to be modified... (e.g., 128B signature size)
	
	//Note: last two bits of ofType indicate the expiration time
	ofType = (ofType & 0xfc)|(exp &0x03);

	#ifdef _SL_DEBUG_GW
	if(ofType) 
		printf("OFTYPE: %02x -> something unusual is added to the PCB\n",ofType);
	#endif

    if(type){
        pcbMarking newMarking=
        //{aid,1,128,PCB_MARKING_SIZE,ofType,ingress,egress,exp,mac,tdid,bwAlloc,0,0};
        {aid,1,sigLen,PCB_MARKING_SIZE,ofType,ingress,egress,mac,tdid,bwAlloc,0,0};
        memcpy(pkt+totalLen, (uint8_t*)&newMarking, PCB_MARKING_SIZE);
        SPH::addTotalLen(pkt, PCB_MARKING_SIZE);
        addNumHops(pkt ,1);
    }else{
        peerMarking newMarking = {aid, ofType, ingress, egress, mac,tdid,bwAlloc,0,0};
        memcpy(pkt+totalLen, (uint8_t*)&newMarking, PEER_MARKING_SIZE);
        mrkPtr->blkSize += PEER_MARKING_SIZE;
        SPH::addTotalLen(pkt, PEER_MARKING_SIZE);
    }
    
    return SCION_SUCCESS;
}


/*
    SCIONBeaconLib::addPeer
    - adds peer link information to the beacon
*/
#ifdef ENABLE_AESNI
uint8_t SCIONBeaconLib::addPeer(uint8_t* pkt, uint16_t ingress, uint16_t egress, uint16_t
pegress, uint8_t type, uint64_t aid, uint32_t tdid, keystruct* aesnikey,uint8_t
ofType, uint8_t exp, uint16_t bwAlloc)
#else
uint8_t SCIONBeaconLib::addPeer(uint8_t* pkt, uint16_t ingress, uint16_t egress, uint16_t
pegress, uint8_t type, uint64_t aid, uint32_t tdid, aes_context* ctx,uint8_t
ofType, uint8_t exp, uint16_t bwAlloc)
#endif
{
    uint16_t totalLen = SPH::getTotalLen(pkt);
	uint32_t ts = SPH::getTimestamp(pkt);
    uint8_t hops = getNumHops(pkt);
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    uint8_t* mrkItr = (uint8_t*)(pkt+hdrLen+OPAQUE_FIELD_SIZE*2); 
    pcbMarking* mrkPtr = (pcbMarking*)mrkItr;
    
	//SL: have to find an efficient way to add peer...
	//run this for every peering links may be unnecessary (though not much)

    for(int i=0;i<hops-1;i++){
        mrkItr += (mrkPtr->blkSize + mrkPtr->sigLen);
        mrkPtr = (pcbMarking*)(mrkItr);
    }
    
    uint64_t oldMAC = mrkPtr->mac;
    if(type==PCB_TYPE_CORE){
        oldMAC = 0;
    }
#ifdef ENABLE_AESNI
	uint64_t mac = createMAC(ts, exp, ingress, egress, oldMAC, aesnikey);
#else
    uint64_t mac = createMAC(ts, exp, ingress, egress, oldMAC, ctx);
#endif
	//SL: This part is unnecessary...
	/*
    if(type){
        pcbMarking newMarking= {aid,ingress,egress,128,PCB_MARKING_SIZE,mac,tdid};
        memcpy(pkt+totalLen, (uint8_t*)&newMarking, PCB_MARKING_SIZE);
        SPH::addPacketLen(pkt, PCB_MARKING_SIZE);
        SPH::addNumOF(pkt ,1); 
    }else{ //SL: This part is adding a peering link
	*/
	peerMarking* pm = (peerMarking*)(pkt+totalLen);
	//SL: why is this?
	//A peering link doesn't add AID to the marking
	//It adds TDID (instead of ADAID) to the marking for supporting inter-TD peering links
	//Just in case the client elements use this, I leave it as it is...
	//Also, since we don't need LSign and Block Size for peering links, TDID can use these fields if the PCB size is of concern. 
	pm->aid= aid;
    pm->type = (ofType&0xfc)|(exp&0x03);
	pm->ingressIf = ingress;
	pm->egressIf = pegress;
    //pm->exp = exp;
	pm->mac = mac;
    pm->tdid = tdid;
    pm->bwAlloc = bwAlloc;
	mrkPtr->blkSize += PEER_MARKING_SIZE;
	SPH::addTotalLen(pkt, PEER_MARKING_SIZE);
    
    return SCION_SUCCESS;
}

/*
    SCIONBeaconLib::signPacket
    - generates the signature for the given packet and append the signature.
*/
uint8_t SCIONBeaconLib::signPacket(uint8_t* pkt, int sigLen, uint64_t &next_aid, rsa_context* ctx){
    uint16_t totalLength = SPH::getTotalLen(pkt);
    uint8_t hops = getNumHops(pkt);
    uint8_t newSignature[sigLen];
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    memset(newSignature, 0, sigLen);
    uint8_t* ptr = pkt+hdrLen+OPAQUE_FIELD_SIZE*2;
    pcbMarking* mrkPtr = (pcbMarking*)ptr;
    uint8_t* prevPtr = NULL;
    pcbMarking* prevMrkPtr = NULL;
    
    for(int i=0;i<hops-1;i++){
        if(i==hops-2){
            prevPtr = ptr;
            prevMrkPtr = mrkPtr;
        }
        ptr+=mrkPtr->blkSize+mrkPtr->sigLen;
        mrkPtr = (pcbMarking*)ptr;
    }

    int size =0;
    if(prevPtr==NULL){
        size+=mrkPtr->blkSize;
    }else{
        size += (prevMrkPtr->sigLen + mrkPtr->blkSize);
    }
    
	//SL: AID chaining
	size += SIZE_AID;

	uint8_t msg[size];
    memset(msg, 0, size);
    memcpy(msg, ptr, mrkPtr->blkSize);

	if(prevPtr!=NULL){ //Non-Core
        memcpy(msg+mrkPtr->blkSize, prevPtr+prevMrkPtr->blkSize,
            prevMrkPtr->sigLen);
		
		//SL: AID chaining
		memcpy(msg+mrkPtr->blkSize+prevMrkPtr->sigLen, &next_aid, SIZE_AID);
    } else { //Core
		memcpy(msg+mrkPtr->blkSize, &next_aid, SIZE_AID);
	}
    
    SCIONCryptoLib::genSig(msg, size, pkt+totalLength, sigLen, ctx);
    SPH::addTotalLen(pkt,sigLen);
    
    return SCION_SUCCESS;
}

void SCIONBeaconLib::printPCB(uint8_t* pkt){
    uint16_t totalLength = SPH::getTotalLen(pkt);
    uint8_t hops = SPH::getNumOF(pkt);
    printf("====== PCB PRINT =======\n");
    printf("%d   %d\n",totalLength,hops);
    uint8_t* ptr = (uint8_t*)(pkt+SCION_HEADER_SIZE);
    pcbMarking* mrkPtr = (pcbMarking*)ptr;
    for(int i=0;i<hops;i++){
        printf("Printing Markings from aid=%llu\n",mrkPtr->aid);
        printf("INGRESS IF=%u\n",mrkPtr->ingressIf);
        printf("EGRESS IF=%u\n",mrkPtr->egressIf);
        printf("SIGNATURE_LENGTH=%u\n",mrkPtr->sigLen);
        printf("BLOCK SIZE=%u\n",mrkPtr->blkSize);
        printf("MAC=%llu\n",mrkPtr->mac);
        printf("TDID=%lu\n",mrkPtr->tdid);
        uint16_t numPeer = (mrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
        if(numPeer>0){
            printf("----------------PEERS num=%u\n", numPeer);
        }else{
            printf("NO PEERING LINKS\n");
        }
        peerMarking *peerPtr = (peerMarking*)(mrkPtr+1);
        for(int j=0;j<numPeer;j++){
            printf("PRINTING PEER NUM=%d\n",j);
            printf("NEIGHBOR AID=%llu\n", peerPtr->aid);
            printf("IGNRESS_IF=%u\n", peerPtr->ingressIf);
            printf("EGRESS IF=%u\n",peerPtr->egressIf);
            printf("MAC = %llu\n",peerPtr->mac);
            peerPtr++;
        }
        ptr+=mrkPtr->blkSize+mrkPtr->sigLen;
        mrkPtr=(pcbMarking*)ptr;
    }
    printf("===== END PRINT =====\n");
}


pcbMarking* SCIONBeaconLib::getPcbMark(uint8_t* packet, uint16_t offset){
    return (pcbMarking*)(packet+offset);
}

pcbMarking* SCIONBeaconLib::getNextPcbMark(pcbMarking* mrkPtr){
    uint8_t* ptr = (uint8_t*)mrkPtr;
    ptr+=mrkPtr->blkSize+mrkPtr->sigLen;
    return (pcbMarking*)ptr;
}


pcbMarking* SCIONBeaconLib::getNextPcbMarkNoSig(pcbMarking* mrkPtr){
    uint8_t* ptr = (uint8_t*)mrkPtr;
    ptr+=mrkPtr->blkSize;
    return (pcbMarking*)ptr;
}

peerMarking* SCIONBeaconLib::getPeerMarking(pcbMarking* mrkPtr){
    uint8_t* ptr = (uint8_t*)mrkPtr;
    return (peerMarking*)(ptr+PCB_MARKING_SIZE); 
}

peerMarking* SCIONBeaconLib::getNextPeer(peerMarking* peerPtr){
    uint8_t* ptr = (uint8_t*)peerPtr;
    return (peerMarking*)(ptr+PEER_MARKING_SIZE);
}

void SCIONBeaconLib::initBeaconInfo(uint8_t* pkt, uint32_t timestamp, uint16_t tdid, uint32_t ROTver){
	//SLT: remove default addr size
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    *(uint8_t*)(pkt+hdrLen) = 0x80;
    *(uint32_t*)(pkt+hdrLen+1) = timestamp;
    *(uint16_t*)(pkt+hdrLen+5) = tdid;
    *(uint8_t*)(pkt+hdrLen+7) = 0;
    *(uint8_t*)(pkt+hdrLen+8) = 0xFF;
    *(uint32_t*)(pkt+hdrLen+9) = ROTver;
    *(uint16_t*)(pkt+hdrLen+13) = 0;
    *(uint8_t*)(pkt+hdrLen+15) = 0;
}

void SCIONBeaconLib::setNumHops(uint8_t* pkt, uint8_t hops){
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    *(uint8_t*)(pkt+hdrLen+7) = hops;
}
void SCIONBeaconLib::addNumHops(uint8_t* pkt, uint8_t hops){
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    uint8_t curHops = *(uint8_t*)(pkt+hdrLen+7);
	#ifdef _SL_DEBUG_BS
	printf("curHops=%d, hops=%d, hdrLen=%d\n", curHops, hops, hdrLen);
	#endif
    *(uint8_t*)(pkt+hdrLen+7) = curHops + hops;
}
uint8_t SCIONBeaconLib::getNumHops(uint8_t* pkt){
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    uint8_t curHops = *(uint8_t*)(pkt+hdrLen+7);
    return curHops;
}

uint32_t SCIONBeaconLib::getROTver(uint8_t* pkt){
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    uint32_t ROTver = *(uint32_t*)(pkt+hdrLen+9);
    return ROTver;
}

void SCIONBeaconLib::setROTver(uint8_t* pkt, uint32_t ROTver){
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    *(uint32_t*)(pkt+hdrLen+9) = ROTver;
}

void SCIONBeaconLib::setInterface(uint8_t* pkt, uint16_t interface){
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    *(uint16_t*)(pkt+hdrLen+13) = interface;
}

uint16_t SCIONBeaconLib::getInterface(uint8_t* pkt){
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    return *(uint16_t*)(pkt+hdrLen+13);
}





CLICK_ENDDECLS
ELEMENT_PROVIDES(SCIONBeaconLib)

