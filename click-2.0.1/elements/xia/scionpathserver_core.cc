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

/*change this to corresponding header*/
#include"scionpathserver_core.hh"

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

/*
    SCIONPathServerCore::configure
    - click configureation function.
*/
int SCIONPathServerCore::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile,
        cpEnd) <0){
            click_chatter("ERR: click configuration fail at SCIONPathServerCore.\n");
            click_chatter("ERR: Fault error, exit SCION Network.\n");
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

/*
    SCIONPathServerCore::initialize
    - click variable initialize function.
*/
int SCIONPathServerCore::initialize(ErrorHandler* errh){
    
    Config config;
    config.parseConfigFile((char*)m_sConfigFile.c_str());
    config.getPSLogFilename(m_sLogFile);
    
    m_iLogLevel = config.getLogLevel();
    m_iNumRegisteredPath = config.getNumRegisterPath();
    m_uAdAid = config.getAdAid();
    
    scionPrinter = new SCIONPrint(m_iLogLevel, m_sLogFile);
    parseTopology();
    
    _timer.initialize(this); 
    _timer.schedule_after_sec(10);
    
    return 0;
}

void SCIONPathServerCore::parseTopology(){
    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
}

void SCIONPathServerCore::run_timer(Timer* timer) {
    sendHello();
    _timer.reschedule_after_sec(5);
}

void SCIONPathServerCore::push(int port, Packet *p) {

    TransportHeader thdr(p);
    uint8_t* s_pkt = (uint8_t *) thdr.payload();
    //copy the content of the click packet and kills the click packte
    uint16_t type = SPH::getType(s_pkt);
    uint16_t packetLength = SPH::getTotalLen(s_pkt);
    uint8_t packet[packetLength];

    memset(packet, 0, packetLength);
    memcpy(packet, s_pkt, packetLength);
    p->kill();

    // TODO: copy logic from run_task
    // variables needed to print log
    HostAddr src = SPH::getSrcAddr(packet);
    uint32_t ts = 0; //SPH::getUpTimestamp(packet);

    /*AID_REQ: AID request packet from switch*/
    if(type==PATH_REG){
        
        #ifdef _DEBUG_PS 
        printf("TDC PS: path registration packet received\n");
        #endif
        
        //prints what path is being registered in the log file (should be
        //removed to a separate function).
        uint16_t hops = SCIONBeaconLib::getNumHops(packet);
        uint8_t srcLen = SPH::getSrcLen(packet);
        uint8_t dstLen = SPH::getDstLen(packet);
        //uint8_t hdrLen = SPH::getHdrLen(packet);
        uint8_t hdrLen = SPH::getHdrLen(packet) + 8; // TODO: why are we off by 8?
        uint8_t* ptr = packet+hdrLen+OPAQUE_FIELD_SIZE;
        pcbMarking* mrkPtr = (pcbMarking*)ptr;
        char buf[MAXLINELEN];
        uint16_t offset = 0;
        for(int i=0;i<hops;i++){
            sprintf(buf+offset, "%lu(%u, %u) |"
                ,mrkPtr->aid,mrkPtr->ingressIf,mrkPtr->egressIf);
            offset =strlen(buf); 
            ptr+=mrkPtr->blkSize;
            mrkPtr = (pcbMarking*)ptr;       
        }
        #ifdef _DEBUG_PS 
        printf("\t buf: %s\n",buf);
        scionPrinter->printLog(IH,type,(char *)"PS(%llu:%llu),RECEIVED PATH REGISTRATION: %s\n" ,m_uAdAid, m_uAid,buf);
        #endif

        //parses path and stores
        parsePath(packet); 

    /*PATH_REQ: path request packet form the local path server*/
    }else if(type == PATH_REQ){
        uint8_t hdrLen = SPH::getHdrLen(packet);
        pathInfo* pi = (pathInfo*)(packet+hdrLen);

        uint8_t srcLen = SPH::getSrcLen(packet);
        uint8_t dstLen = SPH::getDstLen(packet);
        specialOpaqueField* sOF =
        (specialOpaqueField*)(packet+COMMON_HEADER_SIZE+srcLen+dstLen);

        
        uint8_t hops = sOF->hops;
        uint16_t pathLength = (hops+1)*OPAQUE_FIELD_SIZE; //from requester to TDC
        uint64_t target = pi->target;
        HostAddr requester = SPH::getSrcAddr(packet);

        //reversing the up path to get the down path
        uint8_t revPath[pathLength];
        memset(revPath, 0, pathLength);
        reversePath(packet+COMMON_HEADER_SIZE+srcLen+dstLen, revPath, hops);
        
        uint8_t* ptr = revPath+OPAQUE_FIELD_SIZE;
        opaqueField* hopPtr = (opaqueField*)ptr;
        
        #ifdef _SL_DEBUG
        for(int i=0;i<hops;i++){
            #ifdef _DEBUG_PS 
            printf("ingress : %lu, egress: %lu\n", hopPtr->ingressIf, hopPtr->egressIf);
            #endif
            ptr+=OPAQUE_FIELD_SIZE;
            hopPtr = (opaqueField*)ptr;
        }
        #endif

        uint16_t interface = ((opaqueField*)(revPath+OPAQUE_FIELD_SIZE))->egressIf;
        int packetLength = COMMON_HEADER_SIZE+pathLength+srcLen+dstLen+PATH_INFO_SIZE; //PATH_REQ packet len.

        //path look up to send to the local path server
        std::multimap<uint64_t, std::multimap<uint32_t, path> >::iterator itr;
        
        for(itr=paths.begin();itr!=paths.end();itr++){
            //when the target is found
            if(itr->first == target){
                std::multimap<uint32_t, path>::iterator itr2;
                //adds necessary information to the packet
                #ifdef _DEBUG_PS
                printf("TDC PS: AD%lu -- #of Registered Paths: %lu\n",target,itr->second.size());
                #endif
                
                for(itr2=itr->second.begin();itr2!=itr->second.end();itr2++){

                    uint16_t pathContentLength = itr2->second.pathLength;
                    uint16_t newPacketLength = packetLength + pathContentLength;
                    uint8_t newPacket[newPacketLength];
                    
                    //1. Set Src/Dest addresses to the requester
                    HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAdAid);
                    memcpy(newPacket, packet, COMMON_HEADER_SIZE);
                    //set packet header
                    SPH::setType(newPacket, PATH_REP);
                    SPH::setSrcAddr(newPacket, srcAddr);
                    SPH::setDstAddr(newPacket,ifid2addr.find(interface)->second);
                    
                    SPH::setHdrLen(newPacket, packetLength-PATH_INFO_SIZE);
                    SPH::setTotalLen(newPacket,newPacketLength);
                    //SLT: this part (setting current OF) needs to be revised...
                    SPH::setCurrOFPtr(newPacket,SCION_ADDR_SIZE*2);
                    SPH::setDownpathFlag(newPacket);
                    //SL: set downpath flag modified...
                    //uint8_t flags = SPH::getFlags(newPacket);
                    //flags ^= 0x80;
                    //SPH::setFlags(newPacket, flags);
                    
                    //2. Opaque field to the requester AD is copied
                    memcpy(newPacket+COMMON_HEADER_SIZE+srcLen+dstLen,revPath,(hops+1)*OPAQUE_FIELD_SIZE); 
                    
                    //fill in the path info
                    pathInfo* pathReply = (pathInfo*)(newPacket+packetLength-PATH_INFO_SIZE);
                    pathReply->target = target;
                    pathReply->timestamp = itr2->first;
                    pathReply->totalLength = pathContentLength;
                    pathReply->numHop = itr2->second.hops;
                    pathReply->option = 0;
                    memcpy(newPacket+packetLength,
                            itr2->second.msg,pathContentLength);

                    //try {
                    char adbuf[41];
                    snprintf(adbuf, 41, "%040lu", requester.numAddr());

                    string dest = "RE ";
                    dest.append(BHID);
                    dest.append(" ");
                    dest.append("AD:");
                    dest.append(adbuf);
                    dest.append(" ");
                    dest.append("HID:");
                    dest.append((const char*)"0000000000000000000000000000000000100000");

                    printf("DEST=%s\n", dest.c_str());

                    //sends reply
                    sendPacket(newPacket, newPacketLength, dest);
                    
                }
            }
        }
    }
    
}

void SCIONPathServerCore::reversePath(uint8_t* path, uint8_t* output, uint8_t hops){
    //copy special of
    uint8_t* ptr = path;
    memcpy(output, ptr, OPAQUE_FIELD_SIZE);

    ptr+=OPAQUE_FIELD_SIZE;
    uint16_t offset = (hops)*OPAQUE_FIELD_SIZE; 
    opaqueField* hopPtr = (opaqueField*)ptr; 
    for(int i=0;i<hops;i++){
        memcpy(output+offset, ptr, OPAQUE_FIELD_SIZE);
        offset-=OPAQUE_FIELD_SIZE;
        ptr+=OPAQUE_FIELD_SIZE;
        hopPtr = (opaqueField*)ptr;
    }
}

void SCIONPathServerCore::parsePath(uint8_t* pkt){
    
    //create new path struct
    path newPath;
    memset(&newPath, 0, sizeof(path)); 
    uint16_t hops = SCIONBeaconLib::getNumHops(pkt);
    //uint8_t headerLength = SPH::getHdrLen(pkt);
    uint8_t headerLength = SPH::getHdrLen(pkt) + 8; // TODO why are we off by 8?
    specialOpaqueField* sOF = (specialOpaqueField*)(pkt+headerLength);
    uint8_t* ptr = pkt+headerLength+OPAQUE_FIELD_SIZE;

    pcbMarking* mrkPtr = (pcbMarking*)ptr;

    //parses all the necessary information
    for(int i=0;i<hops;i++){
        scionPrinter->printLog(IH,(char *)"AID: %d INGRESS: %d EGRESS: %d\n", mrkPtr->aid, mrkPtr->ingressIf, mrkPtr->egressIf);
        newPath.path.push_back(mrkPtr->aid);
        
		uint8_t* ptr2=ptr+PCB_MARKING_SIZE;
        peerMarking* peerPtr = (peerMarking*)ptr2;
		uint8_t numPeers = (mrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
        
		newPath.hops++;
         
        for(int j=0;j<numPeers;j++){
    
            newPath.peers.insert(std::pair<uint64_t,uint64_t>(mrkPtr->aid,peerPtr->aid));
            newPath.numPeers++;
            ptr2+=PEER_MARKING_SIZE;
            peerPtr = (peerMarking*)ptr2;
    
        }
        ptr+=mrkPtr->blkSize;
        mrkPtr = (pcbMarking*)ptr;
    }

    //storing path to the data structure
    //NOTE: this part is going to changed as mentioned above. 
    std::multimap<uint64_t, std::multimap<uint32_t, path> >::iterator itr;
    itr=paths.find(newPath.path.back()); 
	#ifdef _SL_DEBUG_PS
	//printf("NEW PATH REG: paths count=%lu, newpath_adaid=%lu\n", paths.size(), newPath.path.back());
	#endif
    if(itr==paths.end()){
        std::multimap<uint32_t, path> newMap = std::multimap<uint32_t, path>();
        itr=paths.insert(std::pair<uint64_t,std::multimap<uint32_t,path>
        >(newPath.path.back(), newMap));
    }else if(itr->second.size() >= m_iNumRegisteredPath){
		//return;
        free(itr->second.begin()->second.msg);
        itr->second.erase(itr->second.begin());
        if(itr->second.size()==0){
            paths.erase(itr);
        } 
		#ifdef _SL_DEBUG_PS
		//printf("NEW PATH REG: removing the oldest path out of %d paths\n", m_iNumRegisteredPath);
		#endif
    }
    newPath.timestamp = sOF->timestamp;//SPH::getUpTimestamp(pkt);
    uint16_t pathLength =
        SPH::getTotalLen(pkt)-
        SPH::getHdrLen(pkt);
    newPath.msg = (uint8_t*)malloc(pathLength);
    memset(newPath.msg, 0, pathLength);
    memcpy(newPath.msg,
    pkt+headerLength, pathLength);
    newPath.destAID = newPath.path.back();
    newPath.pathLength = pathLength;
    newPath.tdid = sOF->tdid; 
    itr->second.insert(std::pair<uint32_t, path>(newPath.timestamp,newPath));
}



void SCIONPathServerCore::printPaths(){
 /*
   std::multimap<uint64_t, path>::iterator itr;
   printf("Printing paths from path server core");
   for(itr=paths.begin();itr!=paths.end();itr++){
       printf(" target=%llu\n", itr->first);
        std::vector<uint64_t>::iterator itr2;
        for(itr2=itr->second.path.begin();itr2!=itr->second.path.end();itr2++){
            printf("%llu|",*itr2);
        }
        printf("\n");
   }
   */
}

void SCIONPathServerCore::sendHello() {
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

/*
    SCIONPathServer::sendPacket
    - creates packet with the given data and sends to the given port
*/
void SCIONPathServerCore::sendPacket(uint8_t* data, uint16_t data_length, string dest) {

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
    xiah.set_plen(data_length + thdr->hlen()); // XIA payload = transport header + transport-layer data

    q = xiah.encap(q, false);
	output(0).push(q);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONPathServerCore)


