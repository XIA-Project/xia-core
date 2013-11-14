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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/*change this to corresponding header*/
#include"scionclient.hh"


CLICK_DECLS

/*
    SCIONClient::configure
    - click configuration function
    - clients need additaional information about the topology
*/
    
int 
SCIONClient::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "ADAID", cpkM, cpUnsigned64, &m_uAdAid,
        "TARGET", cpkM, cpUnsigned64, &m_uTarget, 
        "PATHSERVER", cpkM, cpUnsigned64, &m_uPsAid,
//        "LOGFILE", cpkM, cpString, &m_sLogFile,
//        "LOGLEVEL", cpkM, cpInteger, &m_iLogLevel,  
        "AID", cpkM, cpUnsigned64, &m_uAid,
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile,
        cpEnd) <0){

    }

    return 0;
}

/*
    SCIONClient::initialize
    - intialize click variables
*/
int SCIONClient::initialize(ErrorHandler* errh){
    _timer.initialize(this); 
    _timer.schedule_after_sec(5);
//    scionPrinter = new SCIONPrint(m_iLogLevel, m_sLogFile.c_str());
    ScheduleInfo::initialize_task(this, &_task, errh);
    printCounter =0;
	parseTopology();
    return 0;
}

void SCIONClient::parseTopology() {
	TopoParser parser;
	parser.loadTopoFile(m_sTopologyFile.c_str());
	parser.parseServers(m_servers);
}

/*
    SCIONClient::run_timer
    - timer runs and send path request packet to the path server local
    - once paths are downloaded from the path server local it stores them
    - calls getAllPaths function to generate end to end paths
*/
void SCIONClient::run_timer(Timer* timer){

    if(printCounter%3==0){
        multimap<uint64_t ,p>::iterator itr;
        //printf("printing up paths num : %d\n", uppath.size());
        for(itr=uppath.begin();itr!=uppath.end();itr++){
            uint8_t* ptr = itr->second.msg;
            
            specialOpaqueField* sOF =
            (specialOpaqueField*)(ptr);
            
            uint8_t numHops = sOF->hops;
            ptr+=OPAQUE_FIELD_SIZE;

            pcbMarking* mrkPtr = (pcbMarking*)ptr;
            for(int i=0;i<numHops;i++){
                //printf("path %d : ingress : %lu,  egress : %lu\n", i,
                //mrkPtr->ingressIf, mrkPtr->egressIf);
                ptr+=mrkPtr->blkSize;
                mrkPtr = (pcbMarking*)ptr;
            }

        }
        
        //printf("printing down paths num : %d\n", downpath.size());
        for(itr=downpath.begin();itr!=downpath.end();itr++){
            uint8_t* ptr = itr->second.msg;
            
            specialOpaqueField* sOF =
            (specialOpaqueField*)(ptr);
            
            uint8_t numHops = sOF->hops;
            ptr+=OPAQUE_FIELD_SIZE;

            pcbMarking* mrkPtr = (pcbMarking*)ptr;
            for(int i=0;i<numHops;i++){
                //printf("path %d : ingress : %lu,  egress : %lu\n", i,
                //mrkPtr->ingressIf, mrkPtr->egressIf);
                ptr+=mrkPtr->blkSize;
                mrkPtr = (pcbMarking*)ptr;
            }

        }

    }
    //sending down path request to the path server
    if(!m_bRval){
        clearDownPaths();
        clearUpPaths();
//        clearPaths();
//        m_bRval=true;
//        m_bMval=false;
        uint16_t packetLength =
        COMMON_HEADER_SIZE+PATH_INFO_SIZE+SCION_ADDR_SIZE*2;
        uint8_t packet[packetLength];
        memset(packet, 0, packetLength);
        SCIONPacketHeader::setType(packet, PATH_REQ_LOCAL);
        
        HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
		//SL:
		//Set the Path Server Address to the destination address
		HostAddr psAddr = m_servers.find(PathServer)->second.addr;
        //HostAddr dstAddr = HostAddr(HOST_ADDR_SCION, 22222);
        
        SCIONPacketHeader::setSrcAddr(packet,srcAddr); 
        SCIONPacketHeader::setDstAddr(packet, psAddr);
		#ifdef _SL_DEBUG_PS
		printf("Client (%llu:%llu): sending downpath request to PS (%llu)\n", m_uAdAid, m_uAid, psAddr.numAddr());
		#endif
        SCIONPacketHeader::setTotalLen(packet, packetLength); 
        SCIONPacketHeader::setHdrLen(packet,
        COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2);

        pathInfo* pathInformation =
        (pathInfo*)(packet+COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2);
        
        pathInformation->target = m_uTarget;
        pathInformation->tdid = 0;
        pathInformation->option = 0;
        
        WritablePacket* out_packet = Packet::make(DEFAULT_HD_ROOM, packet,
                packetLength, DEFAULT_TL_ROOM);
        output(0).push(out_packet);
    }
    


/*
    //print up/down paths and generate end 2 end paths
    if(!m_bMval){
        printUpPaths();
        printDownPaths();
        getAllPaths();
    }
    
    //send packets to all end 2 end paths
    m_bRval=sendToPath();
    if(paths.size()==0 || !m_bRval){
        _timer.schedule_after_sec(10);

    }else{

        //reschedule timer so that all the packet goes in 20 seconds. 
        double s = paths.size();
        double interval = 20000/s;
        _timer.schedule_after_msec(interval);
    }*/
    printCounter++;
    _timer.schedule_after_sec(3);
}


/*
    SCIONClient::printUpPaths
    - prints up paths to log file
*/
void SCIONClient::printUpPaths(){
    char buf[MAXLINELEN];
    memset(buf, 0, MAXLINELEN);
//    scionPrinter->printLog(IH, "Printing up paths %d\n", uppath.size());

    multimap<uint64_t, p>::iterator itr;
    int offset = 2;
    memcpy(buf, "  ",2);
    for(itr=uppath.begin();itr!=uppath.end();itr++){
        uint8_t* ptr = itr->second.msg;
        pcbMarking* hopPtr = (pcbMarking*)ptr;
        uint16_t hops = itr->second.hops;
        for(int i=0;i<hops;i++){
            sprintf(buf+offset, "%llu(%lu,%lu)|",
                    hopPtr->aid,hopPtr->ingressIf,hopPtr->egressIf); 
            offset = strlen(buf);
            ptr+=hopPtr->blkSize;
            hopPtr=(pcbMarking*)ptr;
        }
        strcat(buf,"\n");
//        scionPrinter->printLog(itr->second.timestamp,buf);
        offset=2;
        memset(buf,0,MAXLINELEN);
        memcpy(buf,"  ",2);
    }
}


/*
    SCIONClient::printDownPaths
    - prints down paths to log file
*/
void SCIONClient::printDownPaths(){
    char buf[MAXLINELEN];
    memset(buf, 0, MAXLINELEN);
//    scionPrinter->printLog(IH, "Printing down paths %d\n", downpath.size());

    multimap<uint64_t, p>::iterator itr;
    int offset = 2;
    memcpy(buf, "  ",2);
    for(itr=downpath.begin();itr!=downpath.end();itr++){
        uint8_t* ptr = itr->second.msg;
        pcbMarking* hopPtr = (pcbMarking*)ptr;
        uint16_t hops = itr->second.hops;
        for(int i=0;i<hops;i++){
            sprintf(buf+offset, "%llu(%lu,%lu)|",
                    hopPtr->aid,hopPtr->ingressIf,hopPtr->egressIf); 
            offset = strlen(buf);
            ptr+=hopPtr->blkSize;
            hopPtr=(pcbMarking*)ptr;
        }
        strcat(buf,"\n");
//        scionPrinter->printLog(itr->second.timestamp,buf);
        offset=2;
        memset(buf,0,MAXLINELEN);
        memcpy(buf,"  ",2);
    }
}


/*
    SCIONClient::run_task
    - main routine of the SCION client
*/
bool SCIONClient::run_task(Task* task){

    //copy contents of the packet and kill click packet
    Packet* pkt;
    while ( (pkt = input(0).pull()) ){
        uint16_t packetLength = SCIONPacketHeader::getTotalLen((uint8_t*)pkt->data());
        uint8_t data[packetLength];
        memset(data, 0, packetLength);
        memcpy(data, (uint8_t*)pkt->data(), packetLength);
        pkt->kill();
        
        //variables for log printing
        uint32_t ts =0; 
        uint16_t type = SCIONPacketHeader::getType(data);
        HostAddr src = SCIONPacketHeader::getSrcAddr(data);
        HostAddr dst = SCIONPacketHeader::getDstAddr(data);
        if (dst.numAddr() == m_uAid) dst = HostAddr(HOST_ADDR_SCION, m_uAdAid);

        //AID request from switch
        if (type == AID_REQ) {
            
       		SCIONPacketHeader::setType(data, AID_REP);
			HostAddr addr(HOST_ADDR_SCION,m_uAid);
       		SCIONPacketHeader::setSrcAddr(data, addr);
            
            WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, data,
            packetLength, DEFAULT_TL_ROOM);
            output(0).push(outPacket);

        //DATA packet received
        } else if (type == DATA) {

            //print it to log file
            printf("Data packet recieved %llu\n", m_uAdAid);
        //Up path downloaded from path server
        } else if (type == UP_PATH) {

            
            //parse and store paths
            parse(data, 0); 

        //Down path downloaded from the TDC path server 
        } else if (type==PATH_REP_LOCAL) {
            

            //parse and store path
            parse(data, 1); 
        }
    }
    _task.fast_reschedule();
    return true;
}


/*
    SCIONClient::parse
    - parse paths and store them.
*/
void SCIONClient::parse(uint8_t* pkt, int type){
    
    //struct for path storage
    p newPath;
    memset(&newPath, 0, sizeof(p)); 
    
    //extracting information from packet header
    uint8_t hdrLen = SCIONPacketHeader::getHdrLen(pkt);
    uint16_t totalLength = SCIONPacketHeader::getTotalLen(pkt);
    uint8_t* ptr = pkt+hdrLen;
    pathInfo* pathInformation = (pathInfo*)(ptr);
    specialOpaqueField* sOF = (specialOpaqueField*)(pkt+hdrLen+PATH_INFO_SIZE);
    uint16_t hops = sOF->hops;
    ptr+=PATH_INFO_SIZE+OPAQUE_FIELD_SIZE;
    /*
    if(type){
        hops = pathInformation->numHop;
        ptr+=PATH_INFO_SIZE;
    }else{
        hops = 0;//SCIONPacketHeader::getNumHops(pkt);
    }*/

    //iterate through payload and get hop information
    pcbMarking* mrkPtr = (pcbMarking*)ptr;
/*
    if(type){
        printf("parsing down path\n");
    }else{

        printf("parsing up path\n");
    }*/
    for(int i=0;i<hops;i++){
        newPath.path.push_back(mrkPtr->aid);
        uint8_t* ptr2=ptr+PCB_MARKING_SIZE;

        peerMarking* peerPtr = (peerMarking*)ptr2;
        uint8_t num_peers = (mrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
        newPath.hops++;

        //extracting peer information
        for(int j=0;j<num_peers;j++){
            peer new_peer;
            memset(&new_peer, 0, sizeof(peer));
            new_peer.aid = mrkPtr->aid;
            new_peer.naid = peerPtr->aid;
            new_peer.srcIfid = peerPtr->ingressIf;
            new_peer.dstIfid = peerPtr->egressIf; 
            newPath.peers.insert(std::pair<uint64_t,
                peer>(mrkPtr->aid,new_peer));
            
            newPath.num_peers++;
            ptr2+=PEER_MARKING_SIZE;
            peerPtr = (peerMarking*)ptr2;
        }

        ptr+=mrkPtr->blkSize;//+mrkPtr->sigLen;
        mrkPtr = (pcbMarking*)ptr;
    }

    //storing down path
    uint16_t pathLength = totalLength-hdrLen-PATH_INFO_SIZE;
    newPath.msg= (uint8_t*)malloc(pathLength);
    memcpy(newPath.msg, pkt+hdrLen+PATH_INFO_SIZE, pathLength);
    newPath.pathLength = pathLength;
    newPath.timestamp = sOF->timestamp; 
    
    if (type) {
        downpath.insert(std::pair<uint64_t, p>(pathInformation->target,newPath));

    //storing up path
    } else {/*
        uint16_t totalLength =
            SCIONPacketHeader::getTotalLen(pkt)-SCION_HEADER_SIZE;
        newDwPath.msg = (uint8_t*)malloc(totalLength);
        memcpy(newDwPath.msg, pkt+SCION_HEADER_SIZE, totalLength);
        newDwPath.pathLength = totalLength;
        newDwPath.timestamp = sOF->timestamp;*/
        uppath.insert(std::pair<uint64_t, p>(pathInformation->target,newPath));
    }
}


/*
    SCIONClient::getAllPaths
    - get all end2end paths using up paths and downpaths
*/
void SCIONClient::getAllPaths(){

    multimap<uint64_t, p>::iterator itr;

    for(itr=downpath.begin();itr!=downpath.end();itr++){

        multimap<uint64_t, p>::iterator itr2;
        p downPath = itr->second;
        
        for(itr2=uppath.begin();itr2!=uppath.end();itr2++){
            p upPath = itr2->second;
            getCorePath(downPath, upPath);
            getXovrPath(downPath, upPath);
        }
    }
    if(paths.size()>0)
        m_bMval = true;
}

/*
    SCIONClient::sendToPath
    - send packets to all the existing end2end paths
*/
bool SCIONClient::sendToPath(){

    //if there is no paths then ignores
    if(paths.size()<=0 ){
        return true;
    }

    //iterate and get all paths and send a packet to each path
    multimap<uint64_t, end2end>::iterator itr;

    uint64_t ctr =0;//for demo
    
    for(itr=paths.begin();itr!=paths.end();itr++){

        //if a packet is already sent then ignore this path
        if(itr->second.sent){
            ctr++;
            continue;

        //send to this path
        }else{
            
            //setting packet header and path (list of opaque fields)
            uint16_t totalLength = itr->second.totalLength;
            uint8_t buf[totalLength];
            memset(buf, 0, totalLength);
            memcpy(buf, itr->second.msg, totalLength);
            *(uint64_t*)(buf+totalLength-sizeof(uint64_t))=ctr;

            //setting packet header
            

            //TODO
            HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAdAid);
            HostAddr dstAddr = HostAddr(HOST_ADDR_SCION, m_uTarget);

            SCIONPacketHeader::setSrcAddr(buf,srcAddr);
            SCIONPacketHeader::setDstAddr(buf,dstAddr);


            itr->second.sent=1;
            
//            scionPrinter->//printLog(IH,DATA,ts,m_uAdAid,m_uTarget,"%u,SENT:%llu\n",totalLength,ctr);
            
            WritablePacket* out_packet = 
                Packet::make(DEFAULT_HD_ROOM, buf,totalLength, DEFAULT_TL_ROOM);
            output(0).push(out_packet);
            return true;
        }
    }
    return false;
}



void SCIONClient::printPath(uint8_t* path, uint16_t hops){
    char buf[MAXLINELEN];
    for(int i=0;i<hops;i++){
        memset(buf, 0, MAXLINELEN);
        pathHop* h = (pathHop*)(path+i*PATH_HOP_SIZE);
        sprintf(buf, "ingress: %u, egress: %u, mac: %llu\n", h->ingressIf, h->egressIf, h->mac);
//        scionPrinter->printLog(buf);
    }
}


/*
    SCIONCliet::getCorePath
    - build a path that passes through TDC
*/
void SCIONClient::getCorePath(p downPath, p upPath){

    //variables for path building
    uint8_t* dwPtr = downPath.msg;
    uint8_t* upPtr = upPath.msg;
    uint8_t dwHop = downPath.hops;
    uint8_t upHop = upPath.hops;
    uint8_t fullPath[(upHop+dwHop)*PATH_HOP_SIZE];
    uint8_t up[upHop*PATH_HOP_SIZE];
    uint8_t down[dwHop*PATH_HOP_SIZE];


    uint16_t totalLength =
        (upHop+dwHop)*PATH_HOP_SIZE+SCION_HEADER_SIZE+sizeof(uint64_t);
    memset(up, 0, upHop*PATH_HOP_SIZE);
    memset(down, 0, dwHop*PATH_HOP_SIZE);
    pcbMarking* upMrkPtr = (pcbMarking*)upPtr;
    uint16_t offset = (upHop-1)*PATH_HOP_SIZE;
    
    
    //get opaque field for the up path and put them into the new packet 
    for(int i=0;i<upHop;i++){
       
        pathHop* nhop = (pathHop*)(fullPath+offset);
        nhop->ingressIf = upMrkPtr->ingressIf;
        nhop->egressIf = upMrkPtr->egressIf;
        nhop->mac = upMrkPtr->mac;
        
        offset-=PATH_HOP_SIZE;
        upPtr+=upMrkPtr->blkSize;
        upMrkPtr = (pcbMarking*)upPtr;
    }

    offset = (upHop*PATH_HOP_SIZE);//offset
    
    
    //get opaque field for the down path and put them into the new packet
    pcbMarking* dwMrkPtr = (pcbMarking*)dwPtr;
    for(int i=0;i<dwHop;i++){
        pathHop* nhop = (pathHop*)(fullPath+offset);
        nhop->ingressIf = dwMrkPtr->ingressIf;
        nhop->egressIf = dwMrkPtr->egressIf;
        nhop->mac = dwMrkPtr->mac;
       
        offset+=PATH_HOP_SIZE;
        dwPtr+=dwMrkPtr->blkSize;
        dwMrkPtr = (pcbMarking*)dwPtr;
    }

    
    //variables for packet header setup
    uint16_t interface = ((pathHop*)fullPath)->ingressIf;
    uint32_t upTs = upPath.timestamp;
    uint32_t dwTs = downPath.timestamp;
    uint8_t pkt_data[totalLength];
    memset(pkt_data, 0, totalLength);
    
    
    //setting packet header
    SCIONPacketHeader::setType(pkt_data, DATA);
    SCIONPacketHeader::setTotalLen(pkt_data, totalLength);
//    SCIONPacketHeader::setOfsPtr(pkt_data, 1);
//    SCIONPacketHeader::setHopPtr(pkt_data, 2);
//    SCIONPacketHeader::setUpTimestamp(pkt_data, upTs);
//    SCIONPacketHeader::setDownTimestamp(pkt_data, dwTs);

    memcpy(pkt_data+SCION_HEADER_SIZE, fullPath, (upHop+dwHop)*PATH_HOP_SIZE);


    //storing new end to endpath    
    *(int*)(pkt_data+totalLength-sizeof(uint64_t)) = m_uTarget;
    end2end newPath;
    memset(&newPath, 0, sizeof(end2end));
    newPath.msg = (uint8_t*)malloc(totalLength);
    newPath.totalLength = totalLength;
    newPath.upTs = upTs;
    newPath.downTs = dwTs;
    newPath.hops = upHop+dwHop;
    newPath.sent =0;
    memcpy(newPath.msg, pkt_data, totalLength); 
    paths.insert(pair<uint64_t, end2end>(m_uTarget, newPath));
}


/*
    SCIONClient::getXovrPath
    - build paths that uses peer links or cross over AD
*/
void SCIONClient::getXovrPath(p downPath, p upPath){

    //iterators to build paths
    std::vector<uint64_t> downHops = downPath.path;
    std::vector<uint64_t> upHops = upPath.path; 
    std::vector<uint64_t>::reverse_iterator itr1;
    std::vector<uint64_t>::reverse_iterator itr2;

    //hop counters for both up and down paths
    int numUpHop =1, numDwHop=1;

    bool found = false;
    
    //iteration of up path
    for(itr1=upHops.rbegin();itr1!=upHops.rend();itr1++){
        numDwHop=1;

        //iteration of down hops
        for(itr2=downHops.rbegin();itr2!=downHops.rend();itr2++){

            //if there is an overlapping AD in both paths
            if(*itr1 == *itr2 && !found){

                found = true;
                
                //variables for path construction
                uint8_t* dwPtr = downPath.msg;
                uint8_t* upPtr = upPath.msg;
                uint8_t dwHop = numDwHop==downPath.hops ? numDwHop : numDwHop+1;
                uint8_t upHop = numUpHop==upPath.hops ? numUpHop : numUpHop+1;
                uint8_t fullPath[(upHop+dwHop)*PATH_HOP_SIZE];
                uint8_t up[upHop*PATH_HOP_SIZE];
                uint8_t down[dwHop*PATH_HOP_SIZE];
                uint16_t totalLength =
                    (upHop+dwHop)*PATH_HOP_SIZE+SCION_HEADER_SIZE+sizeof(uint64_t);
                memset(up, 0, upHop*PATH_HOP_SIZE);
                memset(down, 0, dwHop*PATH_HOP_SIZE);
                pcbMarking* upMrkPtr = (pcbMarking*)upPtr;
                uint16_t offset = (upHop-1)*PATH_HOP_SIZE;


                //building opaque field list for the up path
                for(int i=0;i<upPath.hops;i++){

                    //ignore the unused hops
                    if(i<upPath.hops-upHop){
                        upPtr+=upMrkPtr->blkSize;
                        upMrkPtr = (pcbMarking*)upPtr;
                        continue;
                    }
                    
                    //adding the opque fields of the up path
                    pathHop* nhop = (pathHop*)(fullPath+offset);
                    nhop->ingressIf = upMrkPtr->ingressIf;
                    nhop->egressIf = upMrkPtr->egressIf;
                    nhop->mac = upMrkPtr->mac;

                    offset-=PATH_HOP_SIZE;
                    upPtr+=upMrkPtr->blkSize;
                    upMrkPtr = (pcbMarking*)upPtr;
                }

                //set offset in the packet
                offset = (upHop*PATH_HOP_SIZE);


                //building opaque field list for the down path
                pcbMarking* dwMrkPtr = (pcbMarking*)dwPtr;
                for(int i=0;i<downPath.hops;i++){

                    //ignoring the unsued path
                    if(i<downPath.hops-dwHop){
                        dwPtr+=dwMrkPtr->blkSize;
                        dwMrkPtr = (pcbMarking*)dwPtr;
                        continue;
                    }

                    //adding the opaque fields of the down path
                    pathHop* nhop = (pathHop*)(fullPath+offset);
                    nhop->ingressIf = dwMrkPtr->ingressIf;
                    nhop->egressIf = dwMrkPtr->egressIf;
                    nhop->mac = dwMrkPtr->mac;
                    
                    offset+=PATH_HOP_SIZE;
                    dwPtr+=dwMrkPtr->blkSize;
                    dwMrkPtr = (pcbMarking*)dwPtr;
                }


                //variables to setup the packet header
                uint16_t interface = ((pathHop*)fullPath)->ingressIf;
                uint32_t upTs = upPath.timestamp;
                uint32_t dwTs = downPath.timestamp;
                uint8_t pkt_data[totalLength];
                memset(pkt_data, 0, totalLength);


                //settin up the packet header
                SCIONPacketHeader::setType(pkt_data, DATA);
                SCIONPacketHeader::setTotalLen(pkt_data, totalLength);
//                SCIONPacketHeader::setOfsPtr(pkt_data, 1);
//                SCIONPacketHeader::setHopPtr(pkt_data, 2);
//                SCIONPacketHeader::setUpTimestamp(pkt_data, upTs);
//                SCIONPacketHeader::setDownTimestamp(pkt_data, dwTs);
                memcpy(pkt_data+SCION_HEADER_SIZE, fullPath, (upHop+dwHop)*PATH_HOP_SIZE);

                *(int*)(pkt_data+totalLength-sizeof(uint64_t)) = m_uTarget;


                //adding the new end to end path to the data structure             
                end2end newPath;
                memset(&newPath, 0, sizeof(end2end));
                newPath.msg = (uint8_t*)malloc(totalLength);
                newPath.totalLength = totalLength;
                newPath.upTs = upTs;
                newPath.downTs = dwTs;
                memcpy(newPath.msg, pkt_data, totalLength); 
                
                paths.insert(pair<uint64_t, end2end>(m_uTarget, newPath));
            

            //If not crossover but peering link is found
            }else if(downPath.peers.find(*itr2)!=downPath.peers.end() &&
                    upPath.peers.find(*itr1)!=upPath.peers.end()){
               

               // iterators for the path construction
                multimap<uint64_t, peer>::iterator itr3, itr4;
                multimap<uint64_t, peer>::iterator lower1 =
                    upPath.peers.lower_bound(*itr1);
                multimap<uint64_t, peer>::iterator upper1 = 
                    upPath.peers.upper_bound(*itr1);
                multimap<uint64_t, peer>::iterator lower2 = 
                    downPath.peers.lower_bound(*itr2);
                multimap<uint64_t, peer>::iterator upper2 = 
                    downPath.peers.upper_bound(*itr2);
               
               
                //building a path for peering link  
                //nested loops that compare the peering links for the two AD(peer
                //in and peer out) and get which link in both up/down path matches
                for(itr3=lower1;itr3!=upper1;itr3++){
                    for(itr4=lower2;itr4!=upper2;itr4++){

                        //if matching peering link is found
                        if(peer_cmp(itr3->second, itr4->second)){

                            //variables for building path
                            uint8_t* dwPtr = downPath.msg;
                            uint8_t* upPtr = upPath.msg;
                            uint8_t dwHop = numDwHop+1;
                            uint8_t upHop = numUpHop+1;
                            uint8_t fullPath[(upHop+dwHop)*PATH_HOP_SIZE];
                            uint8_t up[upHop*PATH_HOP_SIZE];
                            uint8_t down[dwHop*PATH_HOP_SIZE];
                            uint16_t totalLength =
                                (upHop+dwHop)*PATH_HOP_SIZE+SCION_HEADER_SIZE+sizeof(uint64_t);
              
                            memset(up, 0, upHop*PATH_HOP_SIZE);
                            memset(down, 0, dwHop*PATH_HOP_SIZE);
              
              
                            //build up path
                            pcbMarking* upMrkPtr = (pcbMarking*)upPtr;
                            uint16_t offset = (upHop-1)*PATH_HOP_SIZE;
                            for(int i=0;i<upPath.hops;i++){
                                
                                //ignoring unused hops
                                if(i<upPath.hops-numUpHop){
                                    upPtr+=upMrkPtr->blkSize;
                                    upMrkPtr = (pcbMarking*)upPtr;
                                    continue;
                                
                                //adding peering link opaqeu field
                                }else if(i==upPath.hops-numUpHop){
                                    peerMarking* peerPtr =
                                        (peerMarking*)(upPtr+PCB_MARKING_SIZE);
                                    uint16_t num_peers =
                                        (upMrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
                                    for(int j=0;j<num_peers;j++){

                                        if(peerPtr->ingressIf==itr3->second.srcIfid
                                            && peerPtr->egressIf==itr3->second.dstIfid
                                            &&  peerPtr->aid==itr3->second.naid){
                                            
                                            //add peer link
                                            pathHop *nhop = (pathHop*)(fullPath+offset);
                                            nhop->ingressIf=peerPtr->ingressIf;
                                            nhop->egressIf=upMrkPtr->egressIf;
                                            nhop->mac=peerPtr->mac;
                                            offset-=PATH_HOP_SIZE;
                                            break;
                                        }
                                        peerPtr++;
                                    }
                                }

                                //add the rest of the up path
                                pathHop* nhop = (pathHop*)(fullPath+offset);
                                nhop->ingressIf = upMrkPtr->ingressIf;
                                nhop->egressIf = upMrkPtr->egressIf;
                                nhop->mac = upMrkPtr->mac;
                                
                                offset-=PATH_HOP_SIZE;
                                upPtr+=upMrkPtr->blkSize;
                                upMrkPtr = (pcbMarking*)upPtr;
                            }

                            //set offset
                            offset = (upHop*PATH_HOP_SIZE);
                            
                            
                            //adding down path opaque field 
                            pcbMarking* dwMrkPtr = (pcbMarking*)dwPtr;
                            for(int i=0;i<downPath.hops;i++){

                                //ignore unsued hops
                                if(i<downPath.hops-numDwHop){
                                    dwPtr+=dwMrkPtr->blkSize;
                                    dwMrkPtr = (pcbMarking*)dwPtr;
                                    continue;
                                
                                //add peer link opaque filed from down path
                                }else if(i==downPath.hops-numDwHop){
                                    peerMarking* peerPtr =
                                        (peerMarking*)(dwPtr+PCB_MARKING_SIZE);
                                    uint16_t num_peers =
                                        (dwMrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
                                    for(int j=0;j<num_peers;j++){
                                        if(peerPtr->ingressIf==itr4->second.srcIfid
                                            && peerPtr->egressIf==itr4->second.dstIfid
                                            &&  peerPtr->aid==itr4->second.naid){
                                            
                                            pathHop *nhop = (pathHop*)(fullPath+offset);
                                            nhop->ingressIf=peerPtr->ingressIf;
                                            nhop->egressIf=dwMrkPtr->egressIf;
                                            nhop->mac=peerPtr->mac;
                                            
                                            offset+=PATH_HOP_SIZE;
                                            break;
                                        }
                                        peerPtr++;
                                    }
                                }
                                
                                //add rest of the opaque field from the down path
                                pathHop* nhop = (pathHop*)(fullPath+offset);
                                nhop->ingressIf = dwMrkPtr->ingressIf;
                                nhop->egressIf = dwMrkPtr->egressIf;
                                nhop->mac = dwMrkPtr->mac;
                                
                                offset+=PATH_HOP_SIZE;
                                dwPtr+=dwMrkPtr->blkSize;
                                dwMrkPtr = (pcbMarking*)dwPtr;
                            }


                            //variables for setting packet header
                            uint16_t interface = ((pathHop*)fullPath)->ingressIf;
                            uint32_t upTs = upPath.timestamp;
                            uint32_t dwTs = downPath.timestamp;
                            uint8_t pkt_data[totalLength];
                            memset(pkt_data, 0, totalLength);


                            //setting packet header
                            SCIONPacketHeader::setType(pkt_data, DATA);
                            SCIONPacketHeader::setTotalLen(pkt_data, totalLength);
//                            SCIONPacketHeader::setOfsPtr(pkt_data, 1); 
//                            SCIONPacketHeader::setHopPtr(pkt_data, 2); 
//                            SCIONPacketHeader::setUpTimestamp(pkt_data, upTs);
//                            SCIONPacketHeader::setDownTimestamp(pkt_data, dwTs);
                            memcpy(pkt_data+SCION_HEADER_SIZE, fullPath, (upHop+dwHop)*PATH_HOP_SIZE);


                            //Storing the new end to end path
                            *(int*)(pkt_data+totalLength-sizeof(uint64_t)) = m_uTarget;
                            end2end newPath;
                            memset(&newPath, 0, sizeof(end2end));
                            newPath.msg = (uint8_t*)malloc(totalLength);
                            newPath.totalLength = totalLength;
                            newPath.upTs =upTs;
                            newPath.downTs = dwTs;
                            memcpy(newPath.msg, pkt_data, totalLength); 
                            paths.insert(pair<uint64_t, end2end>(m_uTarget, newPath));
                   
                        }
                    }
                }
            }
            numDwHop++;
        }
        numUpHop++;
    }
}



/*******************************************
    UNUSED FUNCTIONS FOR NOW
********************************************/
bool SCIONClient::peer_cmp(peer up, peer dw){


    return (up.aid==dw.naid && up.naid==dw.aid && up.srcIfid==dw.dstIfid &&
            up.dstIfid==dw.srcIfid);
}


void SCIONClient::getPeerPath(p downPath, p upPath){

}


void SCIONClient::clearDownPaths(){
    multimap<uint64_t, p>::iterator itr;
    for(itr=downpath.begin();itr!=downpath.end();itr++){
        free(itr->second.msg);   
    }
    downpath.clear();
}

void SCIONClient::clearUpPaths(){
    multimap<uint64_t, p>::iterator itr;
    for(itr=uppath.begin();itr!=uppath.end();itr++){
        free(itr->second.msg);
    }
    uppath.clear();
}

void SCIONClient::clearPaths(){
    multimap<uint64_t, end2end>::iterator itr;
    for(itr=paths.begin();itr!=paths.end();itr++){
        free(itr->second.msg);
    }
    paths.clear();

}

/*UNUSED*/
void SCIONClient::sendToAllPath(){
    
    if(paths.size()<=0){
        return;
    }
    
    multimap<uint64_t, end2end>::iterator itr;
    uint64_t ctr =0;
    for(itr=paths.begin();itr!=paths.end();itr++){
        uint16_t totalLength = itr->second.totalLength;
        uint8_t buf[totalLength];
        memset(buf, 0, totalLength);
        memcpy(buf, itr->second.msg, totalLength);
        *(uint64_t*)(buf+totalLength-sizeof(uint64_t))=ctr;

        HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAdAid);
        SCIONPacketHeader::setSrcAddr(buf,srcAddr);

        HostAddr dstAddr = HostAddr(HOST_ADDR_SCION, m_uTarget);
        SCIONPacketHeader::setDstAddr(buf, dstAddr);
        WritablePacket* out_packet = Packet::make(DEFAULT_HD_ROOM, buf,totalLength, DEFAULT_TL_ROOM);
        output(0).push(out_packet);
        ctr++;
    }
    clearDownPaths();
    clearUpPaths();
    clearPaths();
    m_bRval=false;
}

/*Not used*/
void SCIONClient::printAllPath(){
    char buf[MAXLINELEN];
    memset(buf, 0, MAXLINELEN);
    sprintf(buf, "Printing All Path to %d paths\n", paths.size());
//    scionPrinter->printLog(IH, buf);
    multimap<uint64_t, end2end>::iterator itr;
    uint64_t ctr =0;
    for(itr=paths.begin();itr!=paths.end();itr++){
        memset(buf, 0, MAXLINELEN);
        sprintf(buf, "Path #%d",ctr);
//        scionPrinter->printLog(buf);
        ctr++;
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONClient)


