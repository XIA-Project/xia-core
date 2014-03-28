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
//#include <map>
//#include "uppath.hh"
//#include "upqueue.hh"
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
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "AID", cpkM, cpUnsigned64, &m_uAid, 
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile, 
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile,
       cpEnd) <0){

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

	std::multimap<int, ServerElem>::iterator itr;
	for(itr = m_servers.begin(); itr != m_servers.end(); itr++)
		if(itr->second.aid == m_uAid){
			m_Addr = itr->second.addr;
			break;
		}
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
	initializeOutputPort();
}

void SCIONPathServer::push(int port, Packet *p)
{
    TransportHeader thdr(p);

    uint8_t * s_pkt = (uint8_t *) thdr.payload();

    // Temporary for Tenma
    if (s_pkt[0] == '9' && s_pkt[1] == '^')
    {
        string hello = string((const char *)s_pkt, 90);
        scionPrinter->printLog(IH, (char *)"Recieved HELLO: %s\n", hello.c_str());
    }

    //copy the content of the click packet and kills the click packte
    uint16_t type = SPH::getType(s_pkt);
    uint16_t packetLength = SPH::getTotalLen(s_pkt);
    uint8_t packet[packetLength];

    memset(packet, 0, packetLength);
    memcpy(packet, s_pkt, packetLength);
    p->kill();

    // TODO: copy logic from run_task
}

void SCIONPathServer::run_timer(Timer* timer){
    sendHello();
/*    
        uint8_t packet[COMMON_HEADER_SIZE + PATH_INFO_SIZE];
        printf("sending test request\n");
        uint8_t ts = 0;
        uint64_t target = m_uAdAid;
        HostAddr requestId = HostAddr(HOST_ADDR_SCION, 5);
        sendRequest(target, requestId);

        _timer.schedule_after_sec(5);*/

        _timer.reschedule_after_sec(5);
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
    uint16_t totalLength = SPH::getTotalLen(ptr);
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
    SPH::setType(packet, PATH_REQ);
    SPH::setSrcAddr(packet, srcAddr);
    SPH::setDstAddr(packet, dstAddr);
    SPH::setHdrLen(packet, COMMON_HEADER_SIZE+srcLen+dstLen+pathLength);
    SPH::setCurrOFPtr(packet, srcLen+dstLen);
    SPH::setTotalLen(packet,packetLength);


    //sendPacket(packet, packetLength, PORT_TO_SWITCH, TO_SERVER);

	if(pendingDownpathReq.count(target) > 1)
		return 0;
    
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

/*
    SCIONPathServer::run_tas
    - main routine of path server
*/
bool SCIONPathServer::run_task(Task* task){
#if 0
    
    //pulls packet and copy its content and kill click packet
    Packet* inPacket;
    while((inPacket=input(PORT_TO_SWITCH).pull())){
    
		//SL: for IP Encap
		uint8_t * s_pkt = (uint8_t *) inPacket->data();
		if(m_vPortInfo[PORT_TO_SWITCH].addr.getType() == HOST_ADDR_IPV4){
			struct ip * p_iph = (struct ip *)s_pkt;
			struct udphdr * p_udph = (struct udphdr *)(p_iph+1);
			if(p_iph->ip_p != SCION_PROTO_NUM || ntohs(p_udph->dest) != SCION_PORT_NUM) {
				inPacket->kill();
				return true;
			}
			s_pkt += IPHDR_LEN;
		}

        uint16_t totalLength = SPH::getTotalLen(s_pkt);
        uint8_t packet[totalLength];
        memcpy(packet, s_pkt,totalLength);
        uint16_t type = SPH::getType(s_pkt);
        inPacket->kill();


        //variables needed to print log
        HostAddr src = SPH::getSrcAddr(packet);
        HostAddr dst = SPH::getDstAddr(packet);
        if(src == m_clients.begin()->second.addr) 
            src =HostAddr(HOST_ADDR_SCION, m_uAdAid);
        

        /*AID_REQ : AID request from switch*/
		switch(type) {
		case AID_REQ:{
            uint32_t ts = 0;      
            HostAddr dstAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
            
            //set packet header
            SPH::setType(packet, AID_REP);
            SPH::setSrcAddr(packet, dstAddr);
            
            sendPacket(packet, totalLength, 0);
			break;
		}
        /*UP_PATH: up path from pcb server (pcb with sig removed)*/
		case UP_PATH:{
            uint32_t ts = 0;
            //parse up path from the beacon server
            parseUpPath(packet); 
			break;
		}
        /*PATH_REP : path reply from the path server core*/
		case PATH_REP:{
			uint8_t pathbuf[totalLength]; //for multiple packet transmissions.
			uint8_t buf[totalLength]; //for multiple packet transmissions.
            uint8_t hdrLen = SPH::getHdrLen(packet);
            pathInfo* pi = (pathInfo*)(packet+hdrLen);

			#ifdef _SL_DEBUG
            printf("PATH_REP recieved for target AD %llu\n", pi->target);
			#endif

            specialOpaqueField* sOF =
            (specialOpaqueField*)(packet+hdrLen+PATH_INFO_SIZE);
            uint8_t numHops = sOF->hops;
            uint8_t* ptr = packet+hdrLen+PATH_INFO_SIZE+OPAQUE_FIELD_SIZE;
            pcbMarking* optr = (pcbMarking*)ptr;
            HostAddr dstAddr;
			
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
            	SPH::setType(buf, PATH_REP_LOCAL);
			
				HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
            	SPH::setSrcAddr(buf, srcAddr);
            	SPH::setDstAddr(buf, itr->second);
				
				#ifdef _SL_DEBUG
				printf("PS (%llu:%llu): Sending Downpath to Client: %llu\n", m_uAid, m_uAdAid, itr->second.numAddr());
				#endif
				scionPrinter->printLog(IH,type,(char *)"PS (%llu:%llu): Sending Downpath to Client: %llu\n", 
					m_uAdAid, m_uAid, itr->second.numAddr());

            	sendPacket(buf, totalLength, PORT_TO_SWITCH, TO_SERVER);
			}
			//SL:
			//Remove the downpath from the pending table
			pendingDownpathReq.erase(requesters.first, requesters.second);
			break;
        }
		/*PATH_REQ_LOCAL: path request from client*/
		case PATH_REQ_LOCAL:{
            uint8_t ts = 0;

			//put path info in the packet
            uint8_t hdrLen = SPH::getHdrLen(packet);
            pathInfo* pathRequest = (pathInfo*)(packet+hdrLen);
            uint64_t target = pathRequest->target;
            HostAddr requestId = SPH::getSrcAddr(packet);

			scionPrinter->printLog(IH,type,(char *)"PS (%llu:%llu): Request Downpath from Client: %llu\n", 
				m_uAdAid, m_uAid, target);
            //uint8_t packet[COMMON_HEADER_SIZE + hdrLen + PATH_INFO_SIZE];
            sendRequest(target, requestId);
            sendUpPath(requestId);
			break;
        }
		default: 
            printf("Unsupported Packet type : Path Server\n");
			break;
        }
    }
    _task.fast_reschedule();
#endif
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
    uint8_t headerLength = SPH::getHdrLen(pkt);
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
    newUpPath->msg = (uint8_t*)malloc(SPH::getTotalLen(pkt));
    scionHash nhash = createHash(pkt);
    memcpy(newUpPath->msg, pkt, SPH::getTotalLen(pkt));

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
        uint8_t hdrLen = SPH::getHdrLen(currentBestPath.msg);
        uint16_t totalLength =
            SPH::getTotalLen(currentBestPath.msg); 
        uint8_t data[totalLength+PATH_INFO_SIZE];
        memset(data, 0 , totalLength);
        memcpy(data, currentBestPath.msg, hdrLen);
        
        pathInfo* pi = (pathInfo*)(data+hdrLen);
        pi->target = m_uAdAid;

        memcpy(data+hdrLen+PATH_INFO_SIZE, currentBestPath.msg+hdrLen,
        totalLength-hdrLen);

        HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
        //HostAddr dstAddr = clients.begin()->second.addr;
        
        SPH::setType(data, UP_PATH);
        SPH::setSrcAddr(data, srcAddr);
        SPH::setDstAddr(data, requestId);
        SPH::setTotalLen(data, totalLength+PATH_INFO_SIZE);
        //click_chatter("sending uppath");
        //sendPacket(data, totalLength+PATH_INFO_SIZE, PORT_TO_SWITCH, TO_SERVER);
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

/*
    SCIONPathServer::sendPacket
    - Prints log message and sends out click packet to the given port. 
*/

void SCIONPathServer::sendPacket(uint8_t* data, uint16_t data_length, string dest) {
#if 0
    uint16_t type = SPH::getType(data);
    
    HostAddr src = SPH::getSrcAddr(data);
    HostAddr dst = SPH::getDstAddr(data);
    
    if(dst == m_servers.find(BeaconServer)->second.addr || dst ==
        m_servers.find(PathServer)->second.addr || dst==m_clients.begin()->second.addr){
        dst = HostAddr(HOST_ADDR_SCION, m_uAdAid);
    }
	
	//scionPrinter->printLog(IH,type,ts,src,dst,"%u,SENT\n",data_length);
	//SLA:
	uint8_t ipp[data_length+IPHDR_LEN]; 
	if(m_vPortInfo[port].addr.getType() == HOST_ADDR_IPV4) {
		switch(fwd_type) {
		case TO_SERVER:
			if(m_pIPEncap->encap(ipp,data,data_length,SPH::getDstAddr(data).getIPv4Addr()) 
				== SCION_FAILURE)
				return;
		break;
		case TO_ROUTER:{
			uint16_t iface = SPH::getOutgoingInterface(data);
			std::map<uint16_t,HostAddr>::iterator itr = ifid2addr.find(iface);
			if(itr == ifid2addr.end()) return;
			if(m_pIPEncap->encap(ipp,data,data_length,itr->second.getIPv4Addr()) == SCION_FAILURE)
				return;
		} break;
		default: break;
		}
		data = ipp;
	}

    WritablePacket* outPacket= Packet::make(DEFAULT_HD_ROOM, data, data_length, DEFAULT_TL_ROOM);
    output(port).push(outPacket);
#endif

    string src = "RE ";
    src.append(m_AD.c_str());
    src.append(" ");
    src.append(m_HID.c_str());
    src.append(" ");
    src.append(SID_XROUTE);
    
    // scionPrinter->printLog(IH, (char *)"BS(%s)'s src DAG: %s\n", m_AD.c_str(), src.c_str());

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


/*
	SCIONPathServer::initializeOutputPort
	prepare IP header for IP encapsulation
	if the port is assigned an IP address
*/
void SCIONPathServer::initializeOutputPort() {
	
	portInfo p;
	p.addr = m_Addr;
	m_vPortInfo.push_back(p);

	//Initialize port 0; i.e., prepare internal communication
	if(m_Addr.getType() == HOST_ADDR_IPV4) {
		m_pIPEncap = new SCIONIPEncap;
		m_pIPEncap->initialize(m_Addr.getIPv4Addr());
	}
}



void
SCIONPathServer::constructIfid2AddrMap() {
	std::multimap<int, RouterElem>::iterator itr;
	for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
		ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interface.id, itr->second.addr));
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


