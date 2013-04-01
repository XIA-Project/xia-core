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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <functional>
#include <tr1/unordered_map>
#include <list>
//#include <map>
//#include "uppath.hh"
//#include "upqueue.hh"
#include "scionbeacon.hh"
#include "scionpathserver.hh"

#include "define.hh"
#include <iostream>
#include <fstream>

UPQueue::UPQueue(size_t max_size)
    : head(0), tail(0), size(0), maxSize(max_size)
{
    paths = new upPath* [maxSize];
}

UPQueue::~UPQueue()
{
	//SLP: already deleted in dequeue()
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


/*
    SCIONPathServer::configure
    - click configure function for path server
*/
int 
SCIONPathServer::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "AID", cpkM, cpUnsigned64, &m_uAid, 
        "CONFIG", cpkM, cpString, &m_sConfigFile, 
        "TOPOLOGY", cpkM, cpString, &m_sTopologyFile,
       cpEnd) <0){

    }

    return 0;
}

/*
    SCIONPathServer::initialize
    - initialize click variables
*/
int SCIONPathServer::initialize(ErrorHandler* errh){
    ScheduleInfo::initialize_task(this, &_task, errh);
    _timer.initialize(this); 
    _timer.schedule_after_sec(5);
    initVariables();
    return 0;
}

/*
    SCIONPathServer::parseTopology
    - parse topology file using topo parser
*/
void 
SCIONPathServer::parseTopology(){

    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
    parser.parseRouters(m_routers);
    parser.parseClients(m_clients);

}

/*
    SCIONPathServer::initVariables
    - initialize non_click variables
*/
void SCIONPathServer::initVariables(){
    
    Config config;
    config.parseConfigFile((char*)m_sConfigFile.c_str());
    config.getPSLogFilename(m_sLogFile);
    
    m_iQueueSize = config.getPSQueueSize();
    m_iNumRetUP = config.getNumRetUP();
    m_iLogLevel = config.getLogLevel();
    m_uAdAid = config.getAdAid();
    scionPrinter = new SCIONPrint(m_iLogLevel, m_sLogFile);
    parseTopology();
    constructIfid2AddrMap();
}


void SCIONPathServer::run_timer(Timer* timer){
/*    
        uint8_t packet[COMMON_HEADER_SIZE + PATH_INFO_SIZE];
        printf("sending test request\n");
        uint8_t ts = 0;
        uint64_t target = m_uAdAid;
        HostAddr requestId = HostAddr(HOST_ADDR_SCION, 5);
        sendRequest(target, requestId);

        _timer.schedule_after_sec(5);*/
}


/*
    SCIONPathServer::sendRequest
    - sends request to the TDC Path server for down path to 'target' ad
*/
int SCIONPathServer::sendRequest(uint64_t target, HostAddr requestAid){
    if(upPaths.size()<=0){
        printf("No up-path exists. \n");
        return 0;
    }
	
	//SL: ToDo
	//add requestAid to the pending request table
	//have to decide how often the same path is requested to TDC
	time_t curTime;
	curTime = time(NULL);

	pendingDownpathReq.insert(std::pair<uint64_t, HostAddr>(target, requestAid));
	#ifdef _SL_DEBUG
	printf("PS (%llu:%llu): Downpath request to AD: %llu by Client: %llu, count = %d\n", 
		m_uAid, m_uAdAid, target, requestAid.numAddr(), pendingDownpathReq.count(target));
	#endif

	std::tr1::unordered_map<scionHash, UPQueue*, PathHash>::iterator itr;
    itr = upPaths.begin();

	//SL: msg is the PCB from the TDC
    uint8_t* ptr = itr->second->tailPath()->msg;
    uint16_t hops = itr->second->tailPath()->hops;
    

    /*build up path (of list) using up paths*/
    uint16_t pathLength = (hops+1)*OPAQUE_FIELD_SIZE; //add 1 for the timestamp OF
    uint8_t path[pathLength];
    uint16_t totalLength = SCIONPacketHeader::getTotalLen(ptr);
    memset(path, 0, pathLength);
    buildPath(ptr, path);
    
	//SLT:
	uint8_t srcLen = SCION_ADDR_SIZE;
	uint8_t dstLen = SCION_ADDR_SIZE;

    //make new packet and append the of list
    int packetLength=COMMON_HEADER_SIZE+pathLength+PATH_INFO_SIZE+
        srcLen+dstLen;
    uint16_t interface = 
        ((opaqueField*)(path+OPAQUE_FIELD_SIZE))->ingressIf; //ingress interface to itself
    uint8_t packet[packetLength];
    memset(packet, 0, packetLength);
    memcpy(packet, ptr, COMMON_HEADER_SIZE); 
    memcpy(packet+COMMON_HEADER_SIZE+srcLen+dstLen, path, pathLength);
   
    //put path information (target aid, tdid, option, etc)
    pathInfo* pathRequest = 
        (pathInfo*)(packet+COMMON_HEADER_SIZE +pathLength+srcLen+dstLen); 
    pathRequest->target = target;
    pathRequest->tdid = 0;
    pathRequest->option=0;
   
    //Set Src Addr
    HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAdAid);
    HostAddr dstAddr = ifid2addr.find(interface)->second;
//    printf("dest Addr %llu,interface %lu\n", dstAddr.numAddr(),interface);
    //Set packet header
    SCIONPacketHeader::setType(packet, PATH_REQ);
    SCIONPacketHeader::setSrcAddr(packet, srcAddr);
    SCIONPacketHeader::setDstAddr(packet, dstAddr);
    SCIONPacketHeader::setHdrLen(packet, COMMON_HEADER_SIZE+srcLen+dstLen+pathLength);
    SCIONPacketHeader::setCurrOFPtr(packet, srcLen+dstLen);
    SCIONPacketHeader::setTotalLen(packet,packetLength);


    sendPacket(packet, packetLength, 0);

	if(pendingDownpathReq.count(target) > 1)
		return 0;
    
}

/*
    SCIONPathServer::buildPath
    - build a path (list of opaque field) using path information 
*/
int 
SCIONPathServer::buildPath(uint8_t* pkt, uint8_t* output){

    uint8_t hdrLen = SCIONPacketHeader::getHdrLen(pkt);

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

/*
    SCIONPathServer::run_tas
    - main routine of path server
*/
bool SCIONPathServer::run_task(Task* task){
    
    //pulls packet and copy its content and kill click packet
    Packet* pkt;
    while((pkt=input(0).pull())){
    
        uint16_t totalLength =
            SCIONPacketHeader::getTotalLen((uint8_t*)pkt->data());
        uint8_t packet[totalLength];
        memcpy(packet, (uint8_t*)pkt->data(),totalLength);
        uint16_t type = SCIONPacketHeader::getType((uint8_t*)pkt->data());
        pkt->kill();


        //variables needed to print log
        HostAddr src = SCIONPacketHeader::getSrcAddr(packet);
        HostAddr dst = SCIONPacketHeader::getDstAddr(packet);
        if(src == m_clients.begin()->second.addr) 
            src =HostAddr(HOST_ADDR_SCION, m_uAdAid);
        

        /*AID_REQ : AID request from switch*/
        if(type==AID_REQ){

            uint32_t ts = 0;      
			//scionPrinter->printLog(IH,type,ts,src,m_uAdAid,"%u,RECEIVED\n",totalLength);
           
            HostAddr dstAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
            
            //set packet header
            SCIONPacketHeader::setType(packet, AID_REP);
            SCIONPacketHeader::setSrcAddr(packet, dstAddr);
            
            //set aid
          	//*(uint64_t*)(packet+SCION_HEADER_SIZE) = m_uAid;
            
            sendPacket(packet, totalLength, 0);

        /*UP_PATH: up path from pcb server (pcb with sig removed)*/
        }else if(type==UP_PATH){

            uint32_t ts = 0;
			//scionPrinter->printLog(IH,type,ts,src,m_uAdAid,"%u,RECEIVED\n",totalLength);
            
            //parse up path from the beacon server
            parseUpPath(packet); 

        /*PATH_REP : path reply from the path server core*/
        }else if(type==PATH_REP){
			uint8_t pathbuf[totalLength]; //for multiple packet transmissions.
			uint8_t buf[totalLength]; //for multiple packet transmissions.
            uint8_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
            pathInfo* pi = (pathInfo*)(packet+hdrLen);

			#ifdef _SL_DEBUG
            printf("PATH_REP recieved for target AD %llu\n", pi->target);
			#endif

            specialOpaqueField* sOF =
            (specialOpaqueField*)(packet+hdrLen+PATH_INFO_SIZE);
            uint8_t numHops = sOF->hops;
            uint8_t* ptr = packet+hdrLen+PATH_INFO_SIZE+OPAQUE_FIELD_SIZE;
            pcbMarking* optr = (pcbMarking*)ptr;
            
            HostAddr dstAddr;// = HostAddr(HOST_ADDR_SCION, 111);
			
			//SL:
			//Now, send reply to all clients in the pending request table
			std::multimap<uint64_t,HostAddr>::iterator itr;
			std::pair<std::multimap<uint64_t,HostAddr>::iterator, 
				std::multimap<uint64_t, HostAddr>::iterator> requesters;
			requesters = pendingDownpathReq.equal_range(pi->target);
			#ifdef _SL_DEBUG
			printf("PS (%llu:%llu): Downpath reply to Clients: count = %d\n", 
				m_uAid, m_uAdAid, pendingDownpathReq.count(pi->target));
			#endif
			for(itr = requesters.first; itr != requesters.second; itr++) {
				memcpy(buf, packet, totalLength);
				//SLP: seems unnecessary
            	//parseDownPath(buf);        
            	SCIONPacketHeader::setType(buf, PATH_REP_LOCAL);
			
				HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
            	SCIONPacketHeader::setSrcAddr(buf, srcAddr);
            	SCIONPacketHeader::setDstAddr(buf, itr->second);
				
				#ifdef _SL_DEBUG
				printf("PS (%llu:%llu): Sending Downpath to Client: %llu\n", m_uAid, m_uAdAid, itr->second.numAddr());
				#endif

            	sendPacket(buf, totalLength, 0);

			}
			//SL:
			//Remove the downpath from the pending table
			pendingDownpathReq.erase(requesters.first, requesters.second);

        /*PATH_REQ_LOCAL: path request from client*/
        }else if(type==PATH_REQ_LOCAL){
            uint8_t ts = 0;
//            scionPrinter->printLog(IH,type,ts,src,m_uAdAid,"%u,RECEIVED\n",totalLength);

//put path info in the packet
            uint8_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
            pathInfo* pathRequest = (pathInfo*)(packet+hdrLen);
            uint64_t target = pathRequest->target;
            HostAddr requestId = SCIONPacketHeader::getSrcAddr(packet);

            //uint8_t packet[COMMON_HEADER_SIZE + hdrLen + PATH_INFO_SIZE];
            sendRequest(target, requestId);
            sendUpPath(requestId);

        //unsupported type 
        }else{
            printf("Unsupported Packet type : Path Server\n");
        }
    }
    _task.fast_reschedule();
    return true;
}

/*
   SCIONPathServer::parseUpPath
   - parses up path from the pcb server and stores
*/
void SCIONPathServer::parseUpPath(uint8_t* pkt){
//    printf("parsing up path\n");
    //make new struct
    upPath * newUpPath = new upPath;
    memset(newUpPath, 0, sizeof(upPath));
    uint8_t headerLength = SCIONPacketHeader::getHdrLen(pkt);
    uint8_t* ptr = pkt + headerLength + OPAQUE_FIELD_SIZE; 
	//SL: the first opaque field is a special opaque field for the timestamp
    specialOpaqueField* sOF = (specialOpaqueField*)(pkt+headerLength);
    uint16_t hops = sOF->hops;

    //extract information from the packet and parse them
    pcbMarking* mrkPtr = (pcbMarking*)ptr;

    //hop iteration
	//1. extract AD level path information
    for(int i=0;i<hops;i++){
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

    //save the packet 
	//2. copy opaque fields.
    newUpPath->msg = (uint8_t*)malloc(SCIONPacketHeader::getTotalLen(pkt));
    scionHash nhash = createHash(pkt);
    memcpy(newUpPath->msg, pkt, SCIONPacketHeader::getTotalLen(pkt));

    //if the queue is full then delete 
	//3. store to upPaths -- a series of uppath queues
    if(upPaths.find(nhash) == upPaths.end()) {
		//SLP:
		//if the total # of queues (i.e., paths) exceeds the threshold
		//one of them needs to be removed... (based on a criterion)
		//criterion needs to be defined
		//this subroutine should be defined as a function.
    	if(upPaths.size() >= m_iQueueSize){
        	//remove all paths in the uppath queue
			std::tr1::unordered_map<scionHash, UPQueue*>::iterator pItr;
			//SLP: we have to decide which path to removed
			//currently, the first path in upPaths is removed
			pItr = upPaths.begin();
			//3.1 dequeue paths in a queue
			while(pItr->second->getSize())
				pItr->second->dequeue();
			//3.2 delete queue
			delete pItr->second;
        	upPaths.erase(pItr);
    	}
		//queue length should be defined in the configuration file
        UPQueue* newQueue = new UPQueue(3);
        newQueue->enqueue(newUpPath);
        upPaths.insert(std::pair<scionHash, UPQueue*>(nhash, newQueue));
    }
    else {
        upPaths.find(nhash)->second->enqueue(newUpPath);
    }
}


/*
    SCIONPathServer::sendUpPath
    - send unique up paths to the client
*/
int SCIONPathServer::sendUpPath(const HostAddr &requestId, uint32_t pref){
    
    //if none found then ignore
    if(upPaths.size()==0){
        return 0;
    }

    std::list<upPath> bestPaths;

	//1. Shortest path selection
	//if different criteria are given,
	//that should be taken into account below
	//currently, default is the shortest path (in terms of AD hops)
    if (pref == 0) {
        std::multimap<uint16_t, upPath> shortestPaths;

        //iterate through the up path table and send unique paths
        std::tr1::unordered_map<scionHash, UPQueue*, PathHash>::iterator itr;
        upPath currentUP;
        uint16_t currentUPLen;
		//1.1 find up to the m_iNumRetUP shortest paths
        for(itr = upPaths.begin();itr!=upPaths.end();++itr){
            //SLP: only pick the latest path?
			//return type of tailPath is changed to ptr.
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
        upPath currentShortPath;

		//1.2 add the shortest paths to the bestPaths.
		//why is this step introduced?
        for (spItr; spItr != shortestPaths.end(); ++spItr)
            bestPaths.push_back(spItr->second);
    }

	//2. Send the best paths to the requester (i.e., client)
    std::list<upPath>::iterator bpItr = bestPaths.begin();
    upPath currentBestPath;
    for (bpItr; bpItr != bestPaths.end(); ++bpItr) {
        currentBestPath = *bpItr;
        uint8_t hdrLen = SCIONPacketHeader::getHdrLen(currentBestPath.msg);
        uint16_t totalLength =
            SCIONPacketHeader::getTotalLen(currentBestPath.msg); 
        uint8_t data[totalLength+PATH_INFO_SIZE];
        memset(data, 0 , totalLength);
        memcpy(data, currentBestPath.msg, hdrLen);
        
        pathInfo* pi = (pathInfo*)(data+hdrLen);
        pi->target = m_uAdAid;

        memcpy(data+hdrLen+PATH_INFO_SIZE, currentBestPath.msg+hdrLen,
        totalLength-hdrLen);

        HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
        //HostAddr dstAddr = clients.begin()->second.addr;
        
        SCIONPacketHeader::setType(data, UP_PATH);
        SCIONPacketHeader::setSrcAddr(data, srcAddr);
        SCIONPacketHeader::setDstAddr(data, requestId);
        SCIONPacketHeader::setTotalLen(data, totalLength+PATH_INFO_SIZE);
        //click_chatter("sending uppath");
        sendPacket(data, totalLength+PATH_INFO_SIZE, 0);
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
    
    uint8_t headerLength = SCIONPacketHeader::getHdrLen(pkt);
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
    newDownPath.msg = (uint8_t*)malloc(SCIONPacketHeader::getTotalLen(pkt));
    memcpy(newDownPath.msg, pkt, SCIONPacketHeader::getTotalLen(pkt));
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
            printf("%llu|",*itr2);
        }
        printf("\n");
   }
}

/*
    SCIONPathServer::createHash
    - creates unique ID for a path 
    The difference between this function and the one in pcb server is that
    this function does not include timestamp when creating scionHash. 

    The reason for this is path server should have to update the same path
    with different timestamp and this scionHash is the key to compare.
*/
//SLP: a series of interfaces represents a unique path
//this is already computed in parseUpPath function
//so, needs to be modified... i.e., argument to this function should be a halfPath type.
scionHash SCIONPathServer::createHash(uint8_t* pkt){
    uint8_t hops = SCIONBeaconLib::getNumHops(pkt);
    uint16_t outputSize = hops*(sizeof(uint16_t)*2+sizeof(uint64_t));
    uint8_t unit = sizeof(uint16_t)*2+sizeof(uint64_t);
    uint8_t buf[outputSize];
    memset(buf, 0, outputSize);
    
    pcbMarking* hopPtr = (pcbMarking*)(pkt+SCION_HEADER_SIZE);
    
    uint8_t* ptr = pkt+SCION_HEADER_SIZE;
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
/*
    SCIONPathServer::sendPacket
    - Prints log message and sends out click packet to the given port. 
*/

void SCIONPathServer::sendPacket(uint8_t* packet, uint16_t packetLength, int port){
    uint16_t type = SCIONPacketHeader::getType(packet);
    
    HostAddr src = SCIONPacketHeader::getSrcAddr(packet);
    HostAddr dst = SCIONPacketHeader::getDstAddr(packet);
    
    if(dst == m_servers.find(BeaconServer)->second.addr || dst ==
        m_servers.find(PathServer)->second.addr || dst==m_clients.begin()->second.addr){
        dst = HostAddr(HOST_ADDR_SCION, m_uAdAid);
    }
	
	//scionPrinter->printLog(IH,type,ts,src,dst,"%u,SENT\n",packetLength);
	//SLA:
	//IPv4 handling here
	//if addrtype == IPv4, encap with IP header
	//otherwise
    WritablePacket* outPacket= Packet::make(DEFAULT_HD_ROOM, packet, packetLength,DEFAULT_TL_ROOM);
    output(port).push(outPacket);
}



void
SCIONPathServer::constructIfid2AddrMap() {
	std::multimap<int, RouterElem>::iterator itr;
	for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
		ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interfaceID, itr->second.addr));
	}
}

//SLP:
/* clear all up- and down-paths */
void
SCIONPathServer::clearPaths() {

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


