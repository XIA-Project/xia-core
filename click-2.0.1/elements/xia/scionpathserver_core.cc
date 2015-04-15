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


int SCIONPathServerCore::configure(Vector<String> &conf, ErrorHandler *errh) {
    if(cp_va_kparse(conf, this, errh, 
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile,
        cpEnd) <0) {
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


int SCIONPathServerCore::initialize(ErrorHandler* errh){
    
    Config config;
    config.parseConfigFile((char*)m_sConfigFile.c_str());
    config.getPSLogFilename(m_sLogFile);
    
    m_iLogLevel = config.getLogLevel();
    m_iNumRegisteredPath = config.getNumRegisterPath();
    m_uAdAid = config.getAdAid();
    
    scionPrinter = new SCIONPrint(m_iLogLevel, m_sLogFile);
    #ifdef _DEBUG_PS
    scionPrinter->printLog(IH, (char *)"TDC PS (%s:%s) Initializes.\n", 
    m_AD.c_str(), m_HID.c_str());
    #endif
    
    parseTopology();
    #ifdef _DEBUG_PS
    scionPrinter->printLog(IH, (char *)"Parse Topology Done.\n");
    scionPrinter->printLog(IH, (char *)"TDC PS (%s:%s) Initialization Done.\n", 
        m_AD.c_str(), m_HID.c_str());
    #endif
    
    _timer.initialize(this); 
    _timer.schedule_after_sec(10);
    
    return 0;
}

void SCIONPathServerCore::parseTopology(){
    TopoParser parser;
    // TODO: retrieve server information from the controller
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
    uint16_t type = SPH::getType(s_pkt);
    uint16_t packetLength = SPH::getTotalLen(s_pkt);
    uint8_t packet[packetLength];
    memset(packet, 0, packetLength);
    memcpy(packet, s_pkt, packetLength);
    p->kill();

    if(type==PATH_REG){
        
        #ifdef _DEBUG_PS
        scionPrinter->printLog(IH,type,(char*)"TDC PS Received PATH REGISTRATION.\n");
        #endif

        //parses path and store registered paths
        parsePath(packet); 

    }else if(type == PATH_REQ){
        
        #ifdef _DEBUG_PS
        scionPrinter->printLog(IH,type,(char*)"TDC PS Received PATH REQUEST.\n");
        #endif
        
        //parses path request and send back path info
        parsePathReq(packet);
    }
    
}

void SCIONPathServerCore::parsePathReq(uint8_t* pkt){

    uint8_t hdrLen = SPH::getHdrLen(pkt);
    pathInfo* pi = (pathInfo*)(pkt+hdrLen);
    uint64_t target = pi->target;
    HostAddr requester = SPH::getSrcAddr(pkt);
    
    uint8_t srcLen = SPH::getSrcLen(pkt);
    uint8_t dstLen = SPH::getDstLen(pkt);

    //path look up to send to the local path server
    std::multimap<uint64_t, std::multimap<uint32_t, path> >::iterator itr;
        
    for(itr=paths.begin();itr!=paths.end();itr++){
        //when the target is found
        if(itr->first == target){
            std::multimap<uint32_t, path>::iterator itr2;
            #ifdef _DEBUG_PS
            scionPrinter->printLog(IH, (char*)"TDC PS: AD%lu -- # of Registered Paths: %lu\n", 
                target, itr->second.size());
            #endif
                
            for(itr2=itr->second.begin();itr2!=itr->second.end();itr2++){

                uint16_t pathContentLength = itr2->second.pathLength;
                uint16_t newPacketLength = hdrLen + PATH_INFO_SIZE + pathContentLength;
                uint8_t newPacket[newPacketLength];
                memset(newPacket, 0, newPacketLength);
                
                //1. Set Src/Dest addresses to the requester
                scionHeader hdr;
                hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1));
                hdr.dst = requester;
                hdr.cmn.type = PATH_REP;
                hdr.cmn.totalLen = newPacketLength;
                SPH::setHeader(newPacket, hdr);
                    
                //fill in the path info
                pathInfo* pathReply = (pathInfo*)(newPacket+hdrLen);
                pathReply->target = target;
                pathReply->timestamp = itr2->first;
                pathReply->totalLength = pathContentLength;
                pathReply->numHop = itr2->second.hops;
                pathReply->option = 0;
                memcpy(newPacket+hdrLen+PATH_INFO_SIZE, itr2->second.msg, pathContentLength);
                
                char hidbuf[AIP_SIZE+1];
                requester.getAIPAddr((uint8_t*)hidbuf);
                hidbuf[AIP_SIZE] = '\0';
                
                string dest = "RE ";
                dest.append(BHID);
                dest.append(" ");
                // TODO: remove hard code AD here
                dest.append("AD:");
                dest.append((const char*)"0000000000000000000000000000000000000004");
                dest.append(" ");
                dest.append("HID:");
                dest.append(hidbuf);

                sendPacket(newPacket, newPacketLength, dest);
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
    uint8_t headerLength = SPH::getHdrLen(pkt);
    specialOpaqueField* sOF = (specialOpaqueField*)(pkt+headerLength);
    uint8_t* ptr = pkt+headerLength+OPAQUE_FIELD_SIZE;

    pcbMarking* mrkPtr = (pcbMarking*)ptr;

    //parses all the necessary information
    for(int i=0;i<hops;i++){
        #ifdef _DEBUG_PS
        scionPrinter->printLog(IH,(char *)"AID: %d INGRESS: %d EGRESS: %d\n", 
            mrkPtr->aid, mrkPtr->ingressIf, mrkPtr->egressIf);
        #endif
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
	
	#ifdef _DEBUG_PS
	scionPrinter->printLog(IH,(char*)"NEW PATH REG: paths count=%lu, newpath_adaid=%lu\n", 
	    paths.size(), newPath.path.back());
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
		#ifdef _DEBUG_PS
		scionPrinter->printLog(IH,(char*)"NEW PATH REG: removing the oldest path out of %d paths\n", m_iNumRegisteredPath);
		#endif
    }
    newPath.timestamp = sOF->timestamp; //SPH::getUpTimestamp(pkt);
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


