/*****************************************
 * File Name : sample.cc

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 08-02-2012

 * Purpose : 

******************************************/

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


CLICK_DECLS

/*
    SCIONPathServerCore::configure
    - click configureation function.
*/
int SCIONPathServerCore::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "AID", cpkM, cpUnsigned64, &m_uAid, 
        "CONFIG", cpkM, cpString, &m_sConfigFile,
        "TOPOLOGY", cpkM, cpString, &m_sTopologyFile,
       cpEnd) <0){
    }

    return 0;
}

/*
    SCIONPathServerCore::initialize
    - click variable initialize function.
*/
int SCIONPathServerCore::initialize(ErrorHandler* errh){
    ScheduleInfo::initialize_task(this, &_task, errh);
    initVariables();
    return 0;
}


void 
SCIONPathServerCore::parseTopology(){

    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
    parser.parseRouters(m_routers);
}

/*
    SCIONPathServerCore::initVariables
    - intializes the non-click variables.
*/
void SCIONPathServerCore::initVariables(){
    
    Config config;
    config.parseConfigFile((char*)m_sConfigFile.c_str());
    config.getPSLogFilename(m_sLogFile);
    
    m_iLogLevel = config.getLogLevel();
    m_iNumRegisteredPath = config.getNumRegisterPath();
    m_uAdAid = config.getAdAid();
    
    scionPrinter = new SCIONPrint(m_iLogLevel, m_sLogFile);
    parseTopology();
    constructIfid2AddrMap();
}

void
SCIONPathServerCore::constructIfid2AddrMap() {
	std::multimap<int, RouterElem>::iterator itr;
	for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
		ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interfaceID, itr->second.addr));
	}
}

/*
    SCIONPathServerCore::run_task
    - main routine of the TDC path server. 
*/
bool SCIONPathServerCore::run_task(Task* task){

    Packet* inPacket;
    while((inPacket=input(0).pull())){
    
        //copy the content of the click packet and kills the click packte
        uint16_t type = SCIONPacketHeader::getType((uint8_t*)inPacket->data());
        uint16_t packetLength = SCIONPacketHeader::getTotalLen((uint8_t*)inPacket->data());
        uint8_t packet[packetLength];
        memset(packet, 0, packetLength);
        memcpy(packet, (uint8_t*)inPacket->data(), packetLength);
        inPacket->kill();
       
        //variables needed to print log
        HostAddr src = SCIONPacketHeader::getSrcAddr(packet);
        uint32_t ts = 0;//SCIONPacketHeader::getUpTimestamp(packet);

        /*
        if(type!=PATH_REG) 
            scionPrinter->printLog(IH,type,ts,src,m_uAdAid,"%u,RECEIVED\n",packetLength);*/

        /*AID_REQ: AID request packet from switch*/
        if(type==AID_REQ){
            //set packet header
            SCIONPacketHeader::setType(packet, AID_REP);
            HostAddr dstAddr(HOST_ADDR_SCION, m_uAid);
            SCIONPacketHeader::setSrcAddr(packet, dstAddr);

            //append its aid
          	//*(uint64_t*)(packet+SCION_HEADER_SIZE) = m_uAid;

            sendPacket(packet, packetLength,0); 

        /*PATH_REG: path registration packet from local pcb server*/
        }else if(type==PATH_REG){
           	#ifdef _SL_DEBUG 
			printf("TDC PS: path registeration packet received\n");
			#endif
            //prints what path is being registered in the log file (should be
            //removed to a separate function).
            uint16_t hops = SCIONBeaconLib::getNumHops(packet);
			uint8_t srcLen = SCIONPacketHeader::getSrcLen(packet);
			uint8_t dstLen = SCIONPacketHeader::getDstLen(packet);
			uint8_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
            uint8_t* ptr = packet+hdrLen+OPAQUE_FIELD_SIZE;
            pcbMarking* mrkPtr = (pcbMarking*)ptr;
            char buf[MAXLINELEN];
            uint16_t offset = 0;
            for(int i=0;i<hops;i++){
                sprintf(buf+offset, "%llu(%u, %u) |"
                    ,mrkPtr->aid,mrkPtr->ingressIf,mrkPtr->egressIf);
                offset =strlen(buf); 
                ptr+=mrkPtr->blkSize;
                mrkPtr = (pcbMarking*)ptr;       
            }
           	#ifdef _SL_DEBUG 
			printf("\t buf: %s\n",buf);
			#endif
			//scionPrinter->printLog(IH,type,ts,src,m_uAdAid,"%u,RECEIVED PATH: \n%s\n"
			//,packetLength, buf);

            //parses path and stores
            parsePath(packet); 

        /*PATH_REQ: path request packet form the local path server*/
        }else if(type == PATH_REQ){
            uint8_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
            pathInfo* pi = (pathInfo*)(packet+hdrLen);

            uint8_t srcLen = SCIONPacketHeader::getSrcLen(packet);
            uint8_t dstLen = SCIONPacketHeader::getDstLen(packet);
            specialOpaqueField* sOF =
            (specialOpaqueField*)(packet+COMMON_HEADER_SIZE+srcLen+dstLen);

            
            uint8_t hops = sOF->hops;
            uint16_t pathLength = (hops+1)*OPAQUE_FIELD_SIZE; //from requester to TDC
            uint64_t target = pi->target;
            HostAddr requester = SCIONPacketHeader::getSrcAddr(packet);

            //reversing the up path to get the down path
            uint8_t revPath[pathLength];
            memset(revPath, 0, pathLength);
            reversePath(packet+COMMON_HEADER_SIZE+srcLen+dstLen, revPath, hops);
            
			#ifdef _SL_DEBUG
			printf("TDC PS: PATH_REQ received. hops=%d,target=%llu,requester=%llu,srcLen=%d, dstLen=%d, hdrLen=%d\n", 
				hops, target, requester.numAddr(), srcLen, dstLen, hdrLen);
			#endif
            
            uint8_t* ptr = revPath+OPAQUE_FIELD_SIZE;
            opaqueField* hopPtr = (opaqueField*)ptr;
            #ifdef _SL_DEBUG
			for(int i=0;i<hops;i++){
                printf("ingress : %lu, egress: %lu\n", hopPtr->ingressIf, hopPtr->egressIf);
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
                    for(itr2=itr->second.begin();itr2!=itr->second.end();itr2++){

                        uint16_t pathContentLength = itr2->second.pathLength;
                        uint16_t newPacketLength = packetLength + pathContentLength;
                        uint8_t newPacket[newPacketLength];
                        
						//1. Set Src/Dest addresses to the requester
						HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAdAid);
                        memcpy(newPacket, packet, COMMON_HEADER_SIZE);
                        //set packet header
                        SCIONPacketHeader::setType(newPacket, PATH_REP);
                        SCIONPacketHeader::setSrcAddr(newPacket, srcAddr);
                        SCIONPacketHeader::setDstAddr(newPacket,ifid2addr.find(interface)->second);
                        
                        SCIONPacketHeader::setHdrLen(newPacket, packetLength-PATH_INFO_SIZE);
                        SCIONPacketHeader::setTotalLen(newPacket,newPacketLength);
						//SLT: this part (setting current OF) needs to be revised...
                        SCIONPacketHeader::setCurrOFPtr(newPacket,SCION_ADDR_SIZE*2);
                        uint8_t flags = SCIONPacketHeader::getFlags(newPacket);
                        flags ^= 0x80;
                        SCIONPacketHeader::setFlags(newPacket, flags);
						
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

                        //sends reply
                        sendPacket(newPacket, newPacketLength, 0);
                    }
                }
            }
        }else{
            printf("Unsupported Packet type : Path Server\n");
        }
    }
    _task.fast_reschedule();
    return true;
}

/*
    SCIONPathServerCore::reversePath
    - reversing the up path (list of opaque fields) to get the down path.
*/
void SCIONPathServerCore::reversePath(uint8_t* path, uint8_t* output, uint8_t
hops){
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

/*
    SCIONPathServer::parsePath
    - parses registered path and stores

    NOTE: this function is also subject to change according to tha path selection
    routine in the beacon server. This function should be synced with the pcb
    server.
*/

void SCIONPathServerCore::parsePath(uint8_t* pkt){
    
    //create new path struct
    path newPath;
    memset(&newPath, 0, sizeof(path)); 
    uint16_t hops = SCIONBeaconLib::getNumHops(pkt);
    uint8_t headerLength = SCIONPacketHeader::getHdrLen(pkt);
    specialOpaqueField* sOF = (specialOpaqueField*)(pkt+headerLength);
    uint8_t* ptr = pkt+headerLength+OPAQUE_FIELD_SIZE;

    pcbMarking* mrkPtr = (pcbMarking*)ptr;

    //parses all the necessary information
    for(int i=0;i<hops;i++){
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
	#ifdef _SL_DEBUG
	printf("NEW PATH REG: paths count=%d, newpath_adaid=%llu\n", paths.size(), newPath.path.back());
	#endif
    if(itr==paths.end()){
        std::multimap<uint32_t, path> newMap = std::multimap<uint32_t, path>();
        itr=paths.insert(std::pair<uint64_t,std::multimap<uint32_t,path>
        >(newPath.path.back(), newMap));
    }else if(itr->second.size() >= m_iNumRegisteredPath){
		return;
        free(itr->second.begin()->second.msg);
        itr->second.erase(itr->second.begin());
        if(itr->second.size()==0){
            paths.erase(itr);
        } 
    }
    newPath.timestamp = sOF->timestamp;//SCIONPacketHeader::getUpTimestamp(pkt);
    uint16_t pathLength =
        SCIONPacketHeader::getTotalLen(pkt)-
        SCIONPacketHeader::getHdrLen(pkt);
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

/*
    SCIONPathServer::sendPacket
    - creates packet with the given data and sends to the given port
*/
void SCIONPathServerCore::sendPacket(uint8_t* packet, uint16_t packetLength, int port){

    //variables necessary to print log.
    uint16_t type = SCIONPacketHeader::getType(packet);
    HostAddr src = SCIONPacketHeader::getSrcAddr(packet);
    HostAddr dst = SCIONPacketHeader::getDstAddr(packet);
	
	//uint32_t ts = 0;//SCIONPacketHeader::getDownTimestamp(packet);
	//scionPrinter->printLog(IH, type, ts,src,dst,"%u,SENT\n",packetLength);
    
	//SLA:
	//IPv4 handling here
	//if addrtype == IPv4, encap with IP header
	//otherwise
    WritablePacket* outPacket= Packet::make(DEFAULT_HD_ROOM, packet, packetLength,DEFAULT_TL_ROOM);
    output(port).push(outPacket);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONPathServerCore)


