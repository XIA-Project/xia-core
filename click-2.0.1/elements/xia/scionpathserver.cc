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
#include <functional>
#include <tr1/unordered_map>
#include <list>
#include "scionbeacon.hh"
#include "scionpathserver.hh"

#include "define.hh"
#include <iostream>
#include <fstream>

#include <click/xiaheader.hh>
#include <click/xiacontentheader.hh>
#include <click/xiatransportheader.hh>
#include <click/xid.hh>
#include <click/standard/xiaxidinfo.hh>
#include "xiatransport.hh"
#include "xiaxidroutetable.hh"
#include "xtransport.hh"

#define SID_XROUTE  "SID:1110000000000000000000000000000000001112"

UPQueue::UPQueue(size_t max_size)
    : head(0), tail(0), size(0), maxSize(max_size)
{
    paths = new upPath* [maxSize];
}

UPQueue::~UPQueue()
{
    delete[] paths;
}

bool UPQueue::isEmpty() const
{
    return (size == 0);
}

size_t UPQueue::getSize() const
{
    return size;
}

void UPQueue::enqueue(upPath* path)
{
    if (getSize() == maxSize)
        dequeue();
    paths[tail] = path;
    tail = ++tail % maxSize;
    ++size;
}

void UPQueue::dequeue()
{
    if (!isEmpty()) {
		//SLP: clear all path in the queue
		paths[head]->path.clear();
		paths[head]->peers.clear();
		free(paths[head]->msg);
		delete paths[head];

        head = ++head % maxSize;
        --size;
    }
}

upPath* UPQueue::headPath()
{
    return paths[head];
}

upPath* UPQueue::tailPath()
{
	if(tail)
	    return paths[tail-1];
	else
		return paths[maxSize-1];
}

size_t PathHash::operator()(const scionHash &h) const
{
    size_t hashValue = 0;
    unsigned char currentChar;
    for (int i=0; i < sizeof(size_t); ++i) {
        hashValue = (hashValue << (8*sizeof(char))) + h.hashVal[i];
    }
    return hashValue;
}

CLICK_DECLS


int SCIONPathServer::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile, 
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile,
        cpEnd) <0){
    	    click_chatter("ERR: click configuration fail at SCIONPathServer.\n");
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

int SCIONPathServer::initialize(ErrorHandler* errh){

    Config config;
    config.parseConfigFile((char*)m_sConfigFile.c_str());
    config.getPSLogFilename(m_sLogFile);

    m_iQueueSize = config.getPSQueueSize();
    m_iNumRetUP = config.getNumRetUP();
    m_iLogLevel = config.getLogLevel();
    m_uAdAid = config.getAdAid();
    scionPrinter = new SCIONPrint(m_iLogLevel, m_sLogFile);
    
    #ifdef _DEBUG_PS
    scionPrinter->printLog(IH, (char *)"PS (%s:%s) Initializes.\n", 
    m_AD.c_str(), m_HID.c_str());
    #endif
    
    parseTopology();
    #ifdef _DEBUG_PS
    scionPrinter->printLog(IH, (char *)"Parse Topology Done.\n");
    scionPrinter->printLog(IH, (char *)"PS (%s:%s) Initialization Done.\n", 
        m_AD.c_str(), m_HID.c_str());
    #endif
    
    _timer.initialize(this); 
    _timer.schedule_after_sec(10);
    
    return 0;
}

void SCIONPathServer::parseTopology(){
    TopoParser parser;
    // TODO: retrieve topology information from the controller
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
}

void SCIONPathServer::push(int port, Packet *p)
{
    TransportHeader thdr(p);
    uint8_t *s_pkt = (uint8_t *) thdr.payload();
    uint16_t totalLength = SPH::getTotalLen(s_pkt);
    uint8_t packet[totalLength];
    memcpy(packet, s_pkt,totalLength);
    uint16_t type = SPH::getType(s_pkt);
    p->kill();

    switch(type) {
		
        // UP_PATH: up path registration from local beacon server
        case UP_PATH: {
            #ifdef _DEBUG_PS
            scionPrinter->printLog(IH, type, (char *)"Received Up-path from local BS. Print Path: \n");
            #endif
            parseUpPath(packet);
        }
	    break;
		
        // PATH_REP : path reply from the path server core
	case PATH_REP: {

            uint8_t pathbuf[totalLength]; //for multiple packet transmissions.
	    uint8_t buf[totalLength]; //for multiple packet transmissions.
            uint8_t hdrLen = SPH::getHdrLen(packet);
            pathInfo* pi = (pathInfo*)(packet+hdrLen);

            #ifdef _DEBUG_PS
            scionPrinter->printLog(IH, type, (char*)"PATH_REP recieved for target AD %llu\n", pi->target);
            #endif
			
			//Now, send reply to all clients in the pending request table
			std::multimap<uint64_t,HostAddr>::iterator itr;
			
			std::pair<std::multimap<uint64_t,HostAddr>::iterator, 
				std::multimap<uint64_t, HostAddr>::iterator> requesters;
			requesters = pendingDownpathReq.equal_range(pi->target);
			
			for(itr = requesters.first; itr != requesters.second; itr++) {
				
				// directly copy the path reply and forward it to client
				memcpy(buf, packet, totalLength);
				
            	SPH::setType(buf, PATH_REP_LOCAL);
            	SPH::setSrcAddr(buf, HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1)));
            	SPH::setDstAddr(buf, itr->second);
				
				#ifdef _DEBUG_PS
				scionPrinter->printLog(IH,type,(char *)"PS (%llu:%llu): Sending Downpath to Client: %llu\n", 
					m_uAdAid, m_uAid, itr->second.numAddr());
				#endif

                char hidbuf[AIP_SIZE+1];
                itr->second.getAIPAddr((uint8_t*)hidbuf);
                hidbuf[AIP_SIZE] = '\0';

                string dest = "RE ";
                dest.append(BHID);
                dest.append(" ");
                dest.append(m_AD.c_str());
                dest.append(" ");
                dest.append("HID:");
                dest.append(hidbuf);

            	sendPacket(buf, totalLength, dest);
			}
			pendingDownpathReq.erase(requesters.first, requesters.second);
		}
		break;
        
            // PATH_REQ_LOCAL: path request from client
        case PATH_REQ_LOCAL: {
            
            uint8_t ts = 0;
			// put path info in the packet
            uint8_t hdrLen = SPH::getHdrLen(packet);
            pathInfo* pathRequest = (pathInfo*)(packet+hdrLen);
            uint64_t target = pathRequest->target;
            HostAddr requestId = SPH::getSrcAddr(packet);
            
            #ifdef _DEBUG_PS
            scionPrinter->printLog(IH,type,(char *)"PS (%llu:%llu): Request paths for Target AD: %llu\n", 
				m_uAdAid, m_uAid, target);
            #endif
            
            // for down-path
            int num_buffered_requests = sendRequest(target, requestId);
            #ifdef _DEBUG_PS
			scionPrinter->printLog(IH,type,(char *)"PS buffered %d requests for Target AD: %llu\n", 
				num_buffered_requests, target);
			#endif
			
            // for up-path
            sendUpPath(requestId);
        }
            break;
        
        default: 
            break;
    }
    
}

void SCIONPathServer::run_timer(Timer* timer){
    sendHello();
    _timer.reschedule_after_sec(5);
}

int SCIONPathServer::sendRequest(uint64_t target, HostAddr requestAid) {

    if(upPaths.size()<=0){
        #ifdef _DEBUG_PS
        scionPrinter->printLog(IH, (char *)"No down-paths for AD %llu.\n", target);
        #endif
        return 0;
    }
	
	time_t curTime = time(NULL);

    // insert down path 
	pendingDownpathReq.insert(std::pair<uint64_t, HostAddr>(target, requestAid));
	
	#ifdef _DEBUG_PS
	scionPrinter->printLog(IH, (char*)"PS send down-path request for AD: %llu.\n", 
		target);
	#endif

	/*
	std::tr1::unordered_map<scionHash, UPQueue*, PathHash>::iterator itr;
    itr = upPaths.begin();

    uint8_t* ptr = itr->second->tailPath()->msg;
    uint16_t hops = itr->second->tailPath()->hops;

    // build up path (of list) using up paths
    uint16_t pathLength = (hops+1)*OPAQUE_FIELD_SIZE; //add 1 for the timestamp OF
    uint8_t path[pathLength];
    uint16_t totalLength = SPH::getTotalLen(ptr);
    memset(path, 0, pathLength);
    buildPath(ptr, path);
    */
    
    // retrieve a path, send a path request
    uint16_t length = COMMON_HEADER_SIZE + AIP_SIZE*2 + PATH_INFO_SIZE;
    uint8_t packet[length];
    memset(packet, 0, length);
    
    scionHeader hdr;
    hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1));
    hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)(m_servers.find(PathServer)->second.HID));
    hdr.cmn.type = PATH_REQ;
    hdr.cmn.hdrLen = COMMON_HEADER_SIZE + AIP_SIZE*2;
    hdr.cmn.totalLen = length;
    SPH::setHeader(packet, hdr);
        
    pathInfo* pathRequest = (pathInfo*)(packet + COMMON_HEADER_SIZE + AIP_SIZE*2);
    pathRequest->target = target; 
    pathRequest->tdid = 1;
    pathRequest->option = 0;

    string dest = "RE ";
    dest.append(BHID);
    // TODO: remove hardcoded TDC path server
    dest.append(" ");
    dest.append("AD:");
    dest.append((const char*)"0000000000000000000000000000000000000001");
    dest.append(" ");
    dest.append("HID:");
    dest.append((const char*)m_servers.find(PathServer)->second.HID);
    
    sendPacket(packet, length, dest);

    return pendingDownpathReq.count(target);
}

/*
    SCIONPathServer::buildPath
    - build a path (list of opaque field) using path information 
*/
int 
SCIONPathServer::buildPath(uint8_t* pkt, uint8_t* output){

    uint8_t hdrLen = SPH::getHdrLen(pkt);

    memcpy(output, pkt+hdrLen,OPAQUE_FIELD_SIZE);

    uint8_t* ptr = pkt+hdrLen+OPAQUE_FIELD_SIZE; //after 2 special OFs, AD marking comes.
    pcbMarking* mrkPtr = (pcbMarking*)ptr;
    uint16_t hops = SCIONBeaconLib::getNumHops(pkt);
    uint16_t offset = (hops)*OPAQUE_FIELD_SIZE;
    

    for(int i=0;i<hops;i++){
        opaqueField newHop = opaqueField(0x00, mrkPtr->ingressIf,
            mrkPtr->egressIf, 0, mrkPtr->mac);
        
        memcpy(output+offset, &newHop, OPAQUE_FIELD_SIZE);

        ptr+=mrkPtr->blkSize ;
        mrkPtr = (pcbMarking*)ptr;
        offset-=OPAQUE_FIELD_SIZE;

    }
    return 0;
}


void SCIONPathServer::parseUpPath(uint8_t* pkt){

    // prepare path struct
    upPath * newUpPath = new upPath;
    memset(newUpPath, 0, sizeof(upPath));
    uint8_t headerLength = SPH::getHdrLen(pkt);
    uint8_t* ptr = pkt + headerLength + OPAQUE_FIELD_SIZE;
	// the first opaque field is a special opaque field for the timestamp
	uint8_t hops = SCIONBeaconLib::getNumHops(pkt);

    // extract pcb marking from the packet
    pcbMarking* mrkPtr = (pcbMarking*)ptr;

    // hop iteration
    for(int i=0;i<hops;i++){
        #ifdef _DEBUG_PS
        scionPrinter->printLog(IH,(char *)"AID: %d INGRESS: %d EGRESS: %d\n", 
            mrkPtr->aid, mrkPtr->ingressIf, mrkPtr->egressIf);
        #endif
        newUpPath->path.push_back(mrkPtr->aid);
        uint8_t* ptr2=ptr+PCB_MARKING_SIZE;
        peerMarking* peerPtr = (peerMarking*)ptr2;
        uint8_t numPeers = (mrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
        newUpPath->hops++;

        //peer info parsing
        for(int j=0;j<numPeers;j++){
            newUpPath->peers.insert(std::pair<uint64_t,
                uint64_t>(mrkPtr->aid,peerPtr->aid));
            newUpPath->numPeers++;
            ptr2+=PEER_MARKING_SIZE;
            peerPtr = (peerMarking*)ptr2;
        }

        ptr+=mrkPtr->blkSize;
        mrkPtr = (pcbMarking*)ptr;
    }

    // save the packet 
    // copy opaque fields.
    newUpPath->msg = (uint8_t*)malloc(SPH::getTotalLen(pkt));
    // create hash to see if path exists
    scionHash nhash = createHash(pkt);
    memcpy(newUpPath->msg, pkt, SPH::getTotalLen(pkt));

    // if the queue is full then delete 
	// store to upPaths -- a series of uppath queues
    if(upPaths.find(nhash) == upPaths.end()) {
		// if the total # of queues (i.e., paths) exceeds the threshold
		// one of them needs to be removed... (based on a criterion)
		// criterion needs to be defined
		// this subroutine should be defined as a function.
    	if(upPaths.size() >= m_iQueueSize){
        	// remove all paths in the uppath queue
			std::tr1::unordered_map<scionHash, UPQueue*>::iterator pItr;
			// we have to decide which path to removed
			// currently, the first path in upPaths is removed
			pItr = upPaths.begin();
			// dequeue paths in a queue
			while(pItr->second->getSize())
				pItr->second->dequeue();
			// delete queue
			delete pItr->second;
        	upPaths.erase(pItr);
    	}
		// queue length should be defined in the configuration file
        UPQueue* newQueue = new UPQueue(3);
        newQueue->enqueue(newUpPath);
        upPaths.insert(std::pair<scionHash, UPQueue*>(nhash, newQueue));
        scionPrinter->printLog(IH,(char *)"upPaths.insert.\n");
    }
    else {
        #ifdef _DEBUG_PS
        scionPrinter->printLog(IH,(char *)"New path found. Enqueue this path.\n");
        #endif
        upPaths.find(nhash)->second->enqueue(newUpPath);
    }
    
    scionPrinter->printLog(IH, (char*)"parseUpPath done.\n");
}


int SCIONPathServer::sendUpPath(HostAddr &requestId, uint32_t pref) {
    
    //if none found then ignore
    if(upPaths.size()==0){
        #ifdef _DEBUG_PS
        scionPrinter->printLog(IH,(char *)"No up-path available. Wait for local BS.\n");
        #endif
        return 0;
    }

    std::list<upPath> bestPaths;

	// 1. Shortest path selection
    if (pref == 0) {
        std::multimap<uint16_t, upPath> shortestPaths;

        //iterate through the up path table and send unique paths
        std::tr1::unordered_map<scionHash, UPQueue*, PathHash>::iterator itr;
        upPath currentUP;
        uint16_t currentUPLen;
		// 1.1 find up to the m_iNumRetUP shortest paths
        for(itr = upPaths.begin();itr!=upPaths.end();++itr){
            // only pick the latest path
			// return type of tailPath is changed to ptr.
			currentUP = *itr->second->tailPath();
            currentUPLen = currentUP.hops;
            if (shortestPaths.size() <= m_iNumRetUP)
                shortestPaths.insert(std::pair<uint16_t, upPath>(currentUPLen,currentUP));
            else {
                std::multimap<uint16_t, upPath>::iterator sizeCheckItr = shortestPaths.end();
                --sizeCheckItr;
                if (currentUPLen < sizeCheckItr->first) {
                    shortestPaths.erase(sizeCheckItr);
                    shortestPaths.insert(std::pair<uint16_t, upPath>(currentUPLen,currentUP));
                }
            }
        }

        std::multimap<uint16_t, upPath>::iterator spItr = shortestPaths.begin();
		// 1.2 add the shortest paths to the bestPaths.
		// why is this step introduced?
        for (spItr; spItr != shortestPaths.end(); ++spItr)
            bestPaths.push_back(spItr->second);
    }

	//2. Send the best paths to the requester (i.e., client)
    std::list<upPath>::iterator bpItr = bestPaths.begin();
    upPath currentBestPath;
    
    for (bpItr; bpItr != bestPaths.end(); ++bpItr) {
        
        currentBestPath = *bpItr;
        uint8_t hdrLen = SPH::getHdrLen(currentBestPath.msg);
        uint16_t totalLength = SPH::getTotalLen(currentBestPath.msg); 
        uint8_t data[totalLength+PATH_INFO_SIZE];
        memset(data, 0 , totalLength);
        memcpy(data, currentBestPath.msg, hdrLen);
        
        pathInfo* pi = (pathInfo*)(data+hdrLen);
        pi->target = m_uAdAid;

        memcpy(data+hdrLen+PATH_INFO_SIZE, currentBestPath.msg+hdrLen,
        totalLength-hdrLen);
        
        SPH::setType(data, UP_PATH);
        SPH::setSrcAddr(data, HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1)));
        SPH::setDstAddr(data, requestId);
        SPH::setTotalLen(data, totalLength+PATH_INFO_SIZE);
        
        char hidbuf[AIP_SIZE+1];
        requestId.getAIPAddr((uint8_t*)hidbuf);
        hidbuf[AIP_SIZE] = '\0';

        string dest = "RE ";
        dest.append(BHID);
        dest.append(" ");
        dest.append(m_AD.c_str());
        dest.append(" ");
        dest.append("HID:");
        dest.append(hidbuf);

        sendPacket(data, totalLength+PATH_INFO_SIZE, dest);
    }
}

/*
    SCIONPathServer::parseDownPath
    - similar to the parse up path this function parses down paths and stores it
    it basically is a caching function which is currently used. 

    (In the deme there was no caching to show the down paths changes).
*/

void SCIONPathServer::parseDownPath(uint8_t* pkt){
    downPath newDownPath;
    memset(&newDownPath, 0, sizeof(downPath)); 
    
    uint8_t headerLength = SPH::getHdrLen(pkt);
    pathInfo* pi = (pathInfo*)(pkt+headerLength);

    specialOpaqueField* sOF =
    (specialOpaqueField*)(pkt+headerLength+PATH_INFO_SIZE);
    uint16_t hops = sOF->hops;
    
    uint8_t* ptr = pkt + headerLength +PATH_INFO_SIZE +OPAQUE_FIELD_SIZE;
    pcbMarking* mrkPtr = (pcbMarking*)ptr;

    for(int i=0;i<hops;i++){
        newDownPath.path.push_back(mrkPtr->aid);
		#ifdef _SL_DEBUG
        printf("Parsing DP @ PS: ingress: %llu, egress:%llu\n",mrkPtr->ingressIf, mrkPtr->egressIf);
		#endif
        uint8_t* ptr2=ptr+PCB_MARKING_SIZE;
        peerMarking* peerPtr = (peerMarking*)ptr2;
        uint8_t numPeers = (mrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
        newDownPath.hops++;
        for(int j=0;j<numPeers;j++){
            newDownPath.peers.insert(std::pair<uint64_t,
                uint64_t>(mrkPtr->aid,peerPtr->aid));
            newDownPath.numPeers++;
            ptr2+=PEER_MARKING_SIZE;
            peerPtr = (peerMarking*)ptr2;
        }
        ptr+=mrkPtr->blkSize;
        mrkPtr = (pcbMarking*)ptr;
    }
    newDownPath.msg = (uint8_t*)malloc(SPH::getTotalLen(pkt));
    memcpy(newDownPath.msg, pkt, SPH::getTotalLen(pkt));
    newDownPath.destAID = pi->target;
    downPaths.insert(std::pair<uint16_t, downPath>(newDownPath.hops,newDownPath));
}

/*Just for debuggin purpose. will be removed soon*/
void SCIONPathServer::printDownPath(){
   std::multimap<uint16_t, downPath>::iterator itr;
   printf("Printing paths from path server\n");
   for(itr=downPaths.begin();itr!=downPaths.end();itr++){
        std::vector<uint64_t>::iterator itr2;
        for(itr2=itr->second.path.begin();itr2!=itr->second.path.end();itr2++){
            printf("%lu|",*itr2);
        }
        printf("\n");
   }
}

scionHash SCIONPathServer::createHash(uint8_t* pkt){
    uint8_t hops = SCIONBeaconLib::getNumHops(pkt);
    uint16_t outputSize = hops*(sizeof(uint16_t)*2+sizeof(uint64_t));
    uint8_t unit = sizeof(uint16_t)*2+sizeof(uint64_t);
    uint8_t buf[outputSize];
    memset(buf, 0, outputSize);
    
    pcbMarking* hopPtr = (pcbMarking*)(pkt+SPH::getHdrLen(pkt)+OPAQUE_FIELD_SIZE);
    
    uint8_t* ptr = pkt+SPH::getHdrLen(pkt)+OPAQUE_FIELD_SIZE;
    for(int i=0;i<hops;i++){
        *(uint64_t*)(buf+unit*i)=hopPtr->aid;
        *(uint16_t*)(buf+unit*i+sizeof(uint64_t)) = hopPtr->ingressIf;
        *(uint16_t*)(buf+unit*i+sizeof(uint64_t)+sizeof(uint16_t)) 
                    = hopPtr->egressIf;
        ptr+=hopPtr->blkSize;
        hopPtr = (pcbMarking*)ptr;
    }
    uint8_t sha1Hash[SHA1_SIZE];
    memset(sha1Hash, 0,SHA1_SIZE);
    sha1((uint8_t*)buf, outputSize, sha1Hash);

    scionHash newHash = scionHash();
    memset(newHash.hashVal,0, SHA1_SIZE);
    memcpy(newHash.hashVal, sha1Hash, SHA1_SIZE);
    return newHash;
}

void SCIONPathServer::sendHello() {
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

void SCIONPathServer::sendPacket(uint8_t* data, uint16_t data_length, string dest) {

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

void SCIONPathServer::clearPaths() {

	//1. clear up-paths
   	while(upPaths.size() > 0){
       	//free all paths in the uppath queue
		std::tr1::unordered_map<scionHash, UPQueue*>::iterator pItr;
		pItr = upPaths.begin();
		//free(upPaths.begin()->second->tailPath().msg);
		while(pItr->second->getSize())
			pItr->second->dequeue();
		delete pItr->second;
       	upPaths.erase(upPaths.begin());
   	}
	//2. clear down-paths
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONPathServer)


