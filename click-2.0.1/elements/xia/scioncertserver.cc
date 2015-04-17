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
#include "define.hh"
#include "scioncommonlib.hh"

#include <click/xiaheader.hh>
#include <click/xiacontentheader.hh>
#include <click/xiatransportheader.hh>
#include <click/xid.hh>
#include <click/standard/xiaxidinfo.hh>
#include "xiatransport.hh"
#include "xiaxidroutetable.hh"
#include "xtransport.hh"

#define SID_XROUTE  "SID:1110000000000000000000000000000000001112"


/*change this to corresponding header*/
#include "scioncertserver.hh"


CLICK_DECLS

int SCIONCertServer::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh,
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile, 
        "TOPOLOGY_FILE",cpkM, cpString, &m_sTopologyFile,
        cpEnd) <0){
            click_chatter("Fatal Err: click configuration fail at SCIONCertServer.\n");
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

int SCIONCertServer::initialize(ErrorHandler* errh){
    
	// initialization task
	// task 1: parse config file
	Config config;
	config.parseConfigFile((char*)m_sConfigFile.c_str());

	m_uAdAid = config.getAdAid();
	m_uTdAid = config.getTdAid();

	m_iLogLevel =config.getLogLevel();
	config.getCSLogFilename(m_csLogFile);
	config.getROTFilename((char*)m_sROTFile);

	// setup scionPrinter for message logging
	scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile, this->class_name());
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Initializes.\n");
	#endif

	// task 2: parse topology file
	parseTopology();
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Parse Topology Done.\n");
	#endif
	
	// task 3: parse ROT (root of trust) file at local folder
	parseROT();
	scionPrinter->printLog(IH, (char*)"Parse and Verify ROT Done.\n");

	// task4: checking local cert and private key
	_timer.initialize(this); 
    _timer.schedule_after_sec(10);
	
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Initialization Done.\n");
	#endif
	
	return 0;
}

int SCIONCertServer::parseROT(char* loc){	
	ROTParser parser;
	char* fn = NULL;
	ROT tROT;
	
	if(loc)
		fn = loc;
	else
		fn = m_sROTFile;

	if(parser.loadROTFile(fn)!=ROTParseNoError){
	    #ifdef _DEBUG_CS
		scionPrinter->printLog(WH, (char*)"ROT File missing.\n");
		#endif
		return SCION_FAILURE;
	}
	scionPrinter->printLog(IH, (char*)"Load ROT OK.\n");

	if(parser.parse(tROT)!=ROTParseNoError){
	    #ifdef _DEBUG_CS
		scionPrinter->printLog(WH, (char*)"ROT File parsing error.\n");
		#endif
		return SCION_FAILURE;
	}
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Parse ROT OK.\n");
	#endif

	if(parser.verifyROT(tROT)!=ROTParseNoError){
	    #ifdef _DEBUG_CS
		scionPrinter->printLog(WH, (char*)"ROT File parsing error.\n");
		#endif
		return SCION_FAILURE;
	}

	//Store the ROT if verification passed.
	parser.parse(m_ROT);
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Verify ROT OK.\n");
	#endif

	// prepare ROT for delivery
	FILE* rotFile = fopen(fn, "r");
	fseek(rotFile, 0, SEEK_END);
	curROTLen = ftell(rotFile);
	rewind(rotFile);
	curROTRaw = (char*)malloc(curROTLen*sizeof(char));
	
	char buffer[128];
	int offset =0;

	while(!feof(rotFile)){
		memset(buffer, 0, 128);
		fgets(buffer, 128, rotFile);
		int buffLen = strlen(buffer);
		memcpy(curROTRaw+offset, buffer, buffLen);
		offset+=buffLen;
	}
	fclose(rotFile);
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Stored Verified ROT for further delivery.\n");
	#endif
	return SCION_SUCCESS;
}

void SCIONCertServer::parseTopology(){
    TopoParser parser;
    // TODO: retrieve topology from the controller
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
}

void SCIONCertServer::push(int port, Packet *p)
{
    TransportHeader thdr(p);
    uint8_t* s_pkt = (uint8_t *) thdr.payload();
    uint16_t type = SPH::getType(s_pkt);
    uint16_t packetLength = SPH::getTotalLen(s_pkt);
    uint8_t packet[packetLength];
    memset(packet, 0, packetLength);
    memcpy(packet, s_pkt, packetLength);
    
    switch(type) {
        // TODO: finish testing for recursive ROT request/response and Cert request/response
        case ROT_REQ:
            //processROTRequest(newPacket); 
            break;

        case ROT_REP:
            //processROTReply(newPacket); 
            break;

        case ROT_REQ_LOCAL: {
		        
            uint8_t hdrLen = SPH::getHdrLen(packet);
            ROTRequest *req = (ROTRequest *)SPH::getData(packet);
            //1. return ROT  if the requested ROT is available from the local repository
            if(m_ROT.version >= req->currentVersion) {
                sendROT();
                //2. if not, request the ROT to the provider AD
            } else {
            
            /*
            int ret;
		//if the ROT has not been request,  forward the request packet after changing the type to ROT_REQ
		if((ret = isROTRequested(req->currentVersion, SPH::getSrcAddr(packet))) == ROT_REQ_NO) {
			m_ROTRequests.insert(std::pair<uint32_t, HostAddr>(req->currentVersion,SPH::getSrcAddr(packet)));

			#ifdef _SL_DEBUG_CS
			printf("ROT Request by %llu added to pending table\n",SPH::getSrcAddr(packet).numAddr());
			#endif

			uint16_t totalLength = SPH::getTotalLen(packet);
			SPH::setType(packet, ROT_REQ);
		
			//skip the first OF (i.e., TS) and find the egress IF
			opaqueField* of = (opaqueField*)(SPH::getFirstOF(packet)+OPAQUE_FIELD_SIZE);
			HostAddr dstAddr = ifid2addr.find(of->ingressIf)->second;     
		
			SPH::setSrcAddr(packet, m_Addr);
			SPH::setDstAddr(packet, dstAddr);
			SPH::setCurrOFPtr(packet,SCION_ADDR_SIZE*2); //this can be skipped.
			//sendPacket(packet, totalLength, PORT_TO_SWITCH, TO_ROUTER);
			#ifdef _SL_DEBUG_CS
			printf("ifid : %lu , Destination Addr: %llu\n", of->ingressIf, dstAddr.numAddr());
			printf("CS(%llu:%llu): ROT request to upstream AD: current version = %d, new version = %d\n", 
				m_uAdAid, m_uAid, m_ROT.version, req->currentVersion);
			#endif
		//if ROT is already requested, just queue this request 
		//instead of requesting it again.
		} else {
			if (ret == ROT_REQ_OTHER)
				m_ROTRequests.insert(std::pair<uint32_t, HostAddr>(req->currentVersion,SPH::getSrcAddr(packet)));
		}
	}
			*/
		        }
		    }
			break;
			
	    case CERT_REQ: 
		    //send chain
			//processCertificateRequest(packet); 
			break;
			
		case CERT_REP:
			//processCertificateReply(packet); 
			break;

        case CERT_REQ_LOCAL:
            processLocalCertificateRequest(packet); 
            break;
            
        default:
            break;
    }
}

void SCIONCertServer::run_timer(Timer* timer) {
    sendHello();
    _timer.reschedule_after_sec(5);
}

void SCIONCertServer::sendHello() {
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

void SCIONCertServer::sendROT(){

    #ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"received ROT request from local BS.\n");
	#endif
	
	// send to one of beacon server
	if(m_servers.find(BeaconServer)!=m_servers.end()){

	    // prepare ROT reply packet
	    uint8_t hdrLen = COMMON_HEADER_SIZE+AIP_SIZE*2;
	    uint16_t totalLen = hdrLen + sizeof(struct ROTRequest) + curROTLen;
	    uint8_t rotPacket[totalLen];
	    memset(rotPacket, 0, totalLen);
	    // set length and type
	    SPH::setHdrLen(rotPacket, hdrLen);
	    SPH::setType(rotPacket, ROT_REP_LOCAL); 
	    SPH::setTotalLen(rotPacket, totalLen);
	    
	    // fill address
	    HostAddr srcAddr = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1)); 
	    HostAddr dstAddr = HostAddr(HOST_ADDR_AIP, (uint8_t*)(m_servers.find(BeaconServer)->second.HID));
			
		SPH::setSrcAddr(rotPacket, srcAddr);
		SPH::setDstAddr(rotPacket, dstAddr);
			
		string dest = "RE ";
		dest.append(BHID);
		dest.append(" ");
		dest.append("HID:");
		dest.append((const char*)m_servers.find(BeaconServer)->second.HID);
		
		// set default version number as 0
		struct ROTRequest req;
		req.previousVersion = m_ROT.version;
		req.currentVersion = m_ROT.version; 
		*(ROTRequest *)(rotPacket+hdrLen) = req;
		
		memcpy(rotPacket+hdrLen+sizeof(struct ROTRequest), curROTRaw, curROTLen);
		sendPacket(rotPacket, totalLen, dest);
	}else{
	    #ifdef _DEBUG_CS
		scionPrinter->printLog(EH, (char*)"AD (%s) does not has beacon server.\n", m_AD.c_str());
		#endif
	}
}

void SCIONCertServer::processROTRequest(uint8_t * packet) {

	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Received ROT_REQ request from downstream CS.\n");
	#endif

	uint16_t hops = 0; 
	uint16_t hdrLen = SPH::getHdrLen(packet);
	specialOpaqueField* sOF = (specialOpaqueField *)SPH::getFirstOF(packet);
	hops = sOF->hops;

	//ROTRequest* req= (ROTRequest*)SPH::getData(packet);

	uint8_t downPath[(hops+1)*OPAQUE_FIELD_SIZE];
	reversePath(SPH::getFirstOF(packet), downPath, hops);
	
	//1. Set header
	scionHeader hdr;

	hdr.src = HostAddr(HOST_ADDR_SCION, m_uAid);
	hdr.cmn.type = ROT_REP;
	hdr.cmn.hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2+(hops+1)*OPAQUE_FIELD_SIZE;
	hdr.cmn.totalLen = hdr.cmn.hdrLen + sizeof(ROTRequest) + curROTLen;
	
	//2. Set Opaque Fields
	hdr.cmn.timestamp = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
	hdr.n_of =  hops + 1;
	hdr.p_of = downPath;
	
	uint16_t currOFPtr = SPH::getCurrOFPtr(packet);
	uint16_t offset = currOFPtr-SCION_ADDR_SIZE*2;
	hdr.cmn.currOF = SCION_ADDR_SIZE*2+(hops+1)*OPAQUE_FIELD_SIZE-offset;
	currOFPtr = hdr.cmn.currOF;

	opaqueField* of = (opaqueField*)(downPath+currOFPtr-SCION_ADDR_SIZE*2);
	hdr.dst = ifid2addr.find(of->egressIf)->second;
	
	uint8_t buffer[hdr.cmn.totalLen];
	SPH::setHeader(buffer,hdr);
	
	//3. copy ROT
	ROTRequest * req = (ROTRequest*)(buffer+hdr.cmn.hdrLen);
	//copy the ROT request hdr, telling the recipient the request version is being returned
	memcpy(req, SPH::getData(packet), sizeof(ROTRequest));	
	//If the requested ROT version is higher than the currently available one,
	//it indicates something wrong happened (i.e., unverified PCB propagated to a customer AD,
	//implying bogus PCB injection.
	//For now, we assume currentVersion = previousVersion +1. Otherwise, all versions of ROT
	//starting from previousVersion+1 to currentVersion should be provided to the requester.
	if(req->currentVersion != m_ROT.version)
		req->currentVersion = m_ROT.version;
	//copy the ROT
	memcpy(buffer+hdr.cmn.hdrLen+sizeof(ROTRequest), curROTRaw, curROTLen);	

	//4. Send ROT
	//sendPacket(buffer, hdr.cmn.totalLen, PORT_TO_SWITCH,TO_SERVER);
}

void SCIONCertServer::processROTReply(uint8_t * packet) {
	
	ROTRequest *req = (ROTRequest *)SPH::getData(packet);
	//SL:
	//check if the reply contains a higher ROT file
	//otherwise, don't propagate. Possibly, a bogus ROT request...
	//ToDo: should an error message be sent to Hosts and m_ROTRequesters cleared?
	if(req->previousVersion == req->currentVersion) {
		scionPrinter->printLog(EH, (char*)"The requested ROT does not exist.\n");
		m_ROTRequests.clear();
		return;
	}

	//1. verify if the returned ROT is correct
	char nFile[MAX_FILE_LEN];
	memset(nFile, 0, MAX_FILE_LEN);
	sprintf(nFile,"./TD1/Non-TDC/AD%llu/certserver/ROT/rot-td%llu-%d.xml", m_uAdAid, m_uTdAid, req->currentVersion);
	scionPrinter->printLog(IH, (char*)"Update ROT file: %s\n", nFile);
	
	uint16_t packetLength = SPH::getTotalLen(packet);
	uint16_t offset = SPH::getHdrLen(packet)+sizeof(ROTRequest);
    uint16_t rotLen = packetLength-offset;
    // Write to file defined in Config
    FILE* rotFile = fopen(nFile, "w+");
    fwrite(packet+offset, 1, rotLen, rotFile);
    fclose(rotFile);
	
	if(parseROT(nFile) == SCION_FAILURE) {
        scionPrinter->printLog(IH, (char *)"fail to parse ROT.\n");
        // remove file
        remove(nFile);
        return;
    }else{
        scionPrinter->printLog(IH, (char *)"stored received ROT.\n");
        strncpy( m_sROTFile, nFile, MAX_FILE_LEN );
    }
	
	//2. send it to the requester(s)
	std::pair<std::multimap<uint32_t,HostAddr>::iterator,std::multimap<uint32_t,HostAddr>::iterator > range
		= m_ROTRequests.equal_range(req->currentVersion);
	std::multimap<uint32_t, HostAddr>::iterator itr;
	
	for(itr=range.first;itr!=range.second;itr++){
		SPH::setType(packet, ROT_REP_LOCAL);
		SPH::setSrcAddr(packet, HostAddr(HOST_ADDR_SCION,m_uAid));
		SPH::setDstAddr(packet, itr->second);
		#ifdef _SL_DEBUG_CS
		scionPrinter->printLog(IH, (char*)"ROT_REP_LOCAL to BS (%llu).\n", itr->second.numAddr());
		#endif

		//sendPacket(packet, SPH::getTotalLen(packet), PORT_TO_SWITCH, TO_SERVER);
	}

	uint16_t pathLength = SPH::getHdrLen(packet)-COMMON_HEADER_SIZE;
	uint16_t currOFPtr = SPH::getCurrOFPtr(packet);
	if(currOFPtr+OPAQUE_FIELD_SIZE != pathLength){
		//SL: the following two lines are not necessary. will test this later...
		opaqueField* of = (opaqueField*)(packet+COMMON_HEADER_SIZE+currOFPtr);
		SPH::setDstAddr(packet, ifid2addr.find(of->egressIf)->second);
		//sendPacket(packet, SPH::getTotalLen(packet), PORT_TO_SWITCH, TO_ROUTER);
	}
}

void SCIONCertServer::processCertificateRequest(uint8_t * packet) {
    #ifdef _SL_DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Received CERT_REQ request from downstream CS.\n");
	#endif

	uint16_t hops = 0; 
	uint16_t hdrLen = SPH::getHdrLen(packet);
	specialOpaqueField* sOF =(specialOpaqueField*)(packet+SCION_ADDR_SIZE*2+COMMON_HEADER_SIZE);
	hops = sOF->hops;

	certReq* req= (certReq*)(packet+hdrLen);

	certReq newReq = certReq();
	newReq.numTargets=0;
	
	for(int i=0;i<req->numTargets;i++){
		uint64_t target = req->targets[i];
		uint8_t certFile[MAX_FILE_LEN];
		getCertFile(certFile, target);

		uint16_t currOFPtr = SPH::getCurrOFPtr(packet);
		FILE* cFile;

		if((cFile=fopen((const char*)certFile,"r"))==NULL){
		    #ifdef _SL_DEBUG_CS
			scionPrinter->printLog(IH, (char*)"certificate not found, sending up stream.\n");
			#endif

			newReq.targets[newReq.numTargets]=target;
			newReq.numTargets++;
		}else{
		    #ifdef _SL_DEBUG_CS
			scionPrinter->printLog(IH, (char*)"certificate found sending down stream.\n");
			#endif
			
			uint8_t downPath[(hops+1)*OPAQUE_FIELD_SIZE];
			reversePath(packet+COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2, downPath, hops);
			fseek(cFile,0,SEEK_END);
			long cSize = ftell(cFile);
			rewind(cFile);
			
			//1. Set header
			scionHeader hdr;

			hdr.src = HostAddr(HOST_ADDR_SCION, m_uAid);
			hdr.cmn.type = CERT_REP;
			hdr.cmn.hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2+(hops+1)*OPAQUE_FIELD_SIZE;
			hdr.cmn.totalLen = hdr.cmn.hdrLen + CERT_INFO_SIZE + cSize;
			
			//2. Set Opaque Fields
			hdr.cmn.timestamp = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
			hdr.n_of =  hops + 1;
			hdr.p_of = downPath;
			
			currOFPtr-=SCION_ADDR_SIZE*2;
			hdr.cmn.currOF = (hops+1)*OPAQUE_FIELD_SIZE-currOFPtr+SCION_ADDR_SIZE*2;
			currOFPtr = hdr.cmn.currOF;

			opaqueField* of = (opaqueField*)(downPath+currOFPtr-SCION_ADDR_SIZE*2);
			hdr.dst = ifid2addr.find(of->egressIf)->second;
			
			uint8_t buffer[hdr.cmn.totalLen];
			SPH::setHeader(buffer,hdr);
			
			//3. copy certificate
			certInfo* info = (certInfo*)(buffer+hdr.cmn.hdrLen);
			info->target= target;
			info->length = cSize;
			fread(buffer+hdr.cmn.hdrLen+CERT_INFO_SIZE,1,cSize,cFile);
			fclose(cFile);

			//4. send certificate
			//sendPacket(buffer, hdr.cmn.totalLen, PORT_TO_SWITCH,TO_SERVER);
		}
	} // end of for

	if(newReq.numTargets!=0){
		uint16_t outPacketLength = SPH::getTotalLen(packet);
		memcpy(packet+hdrLen,&newReq,CERT_REQ_SIZE);
		//SL: probably unnecessary since this is CERT_REQ from children
		//SPH::setType(packet, CERT_REQ);

		SPH::setSrcAddr(packet, HostAddr(HOST_ADDR_SCION, m_uAid));
		opaqueField* of = (opaqueField*)(packet+SPH::getCurrOFPtr(packet));
		SPH::setDstAddr(packet,ifid2addr.find(of->ingressIf)->second);
		//sendPacket(packet, outPacketLength, PORT_TO_SWITCH,TO_SERVER);
	}
}

void SCIONCertServer::processCertificateReply(uint8_t * packet) {

    #ifdef _SL_DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Received CERT_REP request from upstream CS.\n");
	#endif

	//CERT_REP from upstream CS
	//verify certificate
	uint16_t hops = 0; 
	if(verifyCert(packet)==SCION_SUCCESS){
		uint16_t hdrLen = SPH::getHdrLen(packet);
		certInfo* info = (certInfo*)(packet+hdrLen);
		uint64_t target = info->target;
		uint16_t length = info->length;
		
		std::pair<std::multimap<uint64_t,HostAddr>::iterator,std::multimap<uint64_t,HostAddr>::iterator > range
			= m_certRequests.equal_range(target);
		std::multimap<uint64_t, HostAddr>::iterator itr;
		
		for(itr=range.first;itr!=range.second;itr++){
			scionHeader hdr;

			hdr.cmn.type = CERT_REP_LOCAL;
			hdr.cmn.hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
			uint16_t p_offset = hdr.cmn.hdrLen+CERT_INFO_SIZE;
			hdr.cmn.totalLen = p_offset+info->length;
			uint8_t buffer[hdr.cmn.totalLen];

			hdr.src = HostAddr(HOST_ADDR_SCION, m_uAid);
			hdr.dst = itr->second;
			SPH::setHeader(buffer,hdr);

			info = (certInfo*)(buffer+hdr.cmn.hdrLen);
			info->target = target;
			info->length = length;
			//must call SPH::getHdrLen since the packet can have opaque fields for children ADs.
			memcpy(buffer+p_offset, packet+SPH::getHdrLen(packet)+CERT_INFO_SIZE,length);
			
			WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, buffer, hdr.cmn.totalLen, DEFAULT_TL_ROOM);
			output(PORT_TO_SWITCH).push(outPacket);
		}
	}else{
        #ifdef _SL_DEBUG_CS
		scionPrinter->printLog(IH, (char*)"certificate verification failed.\n", m_uAdAid, m_uAid);
		#endif
	}

	uint16_t pathLength = SPH::getHdrLen(packet)-COMMON_HEADER_SIZE;
	uint16_t currOFPtr = SPH::getCurrOFPtr(packet);
	if(currOFPtr+OPAQUE_FIELD_SIZE != pathLength){
		opaqueField* of = (opaqueField*)(packet+COMMON_HEADER_SIZE+currOFPtr);
		SPH::setDstAddr(packet, ifid2addr.find(of->egressIf)->second);
		WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, packet,SPH::getTotalLen(packet),DEFAULT_TL_ROOM);
		output(PORT_TO_SWITCH).push(outPacket);
	}
}

void SCIONCertServer::processLocalCertificateRequest(uint8_t * packet) {

	HostAddr srcAddr = SPH::getSrcAddr(packet);
	HostAddr dstAddr = SPH::getDstAddr(packet);
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Got CERT_REQ_LOCAL from local BS.\n");
	#endif

	specialOpaqueField* sOF = (specialOpaqueField*)(packet+COMMON_HEADER_SIZE+srcAddr.getLength()+dstAddr.getLength());
	uint16_t hops = sOF->hops; 
	
	// get Cert Request Structure
	certReq* req = (certReq*)(packet+SPH::getHdrLen(packet));
	int numRequest = req->numTargets;
	certReq newReq = certReq();
	newReq.numTargets=0;

	for(int i=0;i<numRequest;i++){
		uint64_t target = req->targets[i];
		#ifdef _DEBUG_CS
		scionPrinter->printLog(IH, (char*)"Request target AID = %llu\n", target);
		#endif
		uint8_t certFile[MAX_FILE_LEN];
		getCertFile(certFile, target);
		
		FILE* cFile;
		if((cFile=fopen((const char*)certFile,"r"))==NULL){
		    #ifdef _DEBUG_CS
			scionPrinter->printLog(IH, (char*)"certificate not found, sending upstream.\n");
			#endif 
			if(!isCertRequested(target, srcAddr)){
				m_certRequests.insert(std::pair<uint64_t, HostAddr>(target,srcAddr));
				newReq.targets[newReq.numTargets]=target;
				newReq.numTargets++;
				// go to label send upstream
			}
		}else{
		    #ifdef _DEBUG_CS
			scionPrinter->printLog(IH, (char*)"found AID %llu cert and send it back to local BS.\n", target);
			#endif 
			
			fseek(cFile,0,SEEK_END);
			long cSize = ftell(cFile);
			rewind(cFile);
			
			// construct packet for certificates
			scionHeader hdr;
			hdr.cmn.hdrLen = COMMON_HEADER_SIZE+AIP_SIZE*2;
			hdr.cmn.totalLen = hdr.cmn.hdrLen+CERT_INFO_SIZE+cSize;
			uint8_t certRepPkt[hdr.cmn.totalLen];
			hdr.cmn.type = CERT_REP_LOCAL;
			hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1)); 
			hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)(m_servers.find(BeaconServer)->second.HID));
			SPH::setHeader(certRepPkt,hdr);

			certInfo* info = (certInfo*)(certRepPkt+hdr.cmn.hdrLen);
			info->target= target;
			info->length = cSize;
			fread(certRepPkt+hdr.cmn.hdrLen+CERT_INFO_SIZE,1,cSize,cFile);
			fclose(cFile);
			
			string dest = "RE ";
			dest.append(BHID);
			dest.append(" ");
			dest.append("HID:");
			dest.append((const char*)m_servers.find(BeaconServer)->second.HID);
			
			sendPacket(certRepPkt, hdr.cmn.totalLen, dest);
		}
	}
	
	// lable::send upstream
	if(newReq.numTargets!=0){
		uint16_t hdrLen = SPH::getHdrLen(packet);
		uint16_t outPacketLength = SPH::getTotalLen(packet);
		memcpy(packet+hdrLen,&newReq,CERT_REQ_SIZE);
		SPH::setType(packet, CERT_REQ);

		opaqueField* of = (opaqueField*)(packet+COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2+OPAQUE_FIELD_SIZE);
		HostAddr dstAddr = ifid2addr.find(of->ingressIf)->second;     
		#ifdef _DEBUG_CS
		scionPrinter->printLog(IH, (char*)"ifid : %lu , destination : %llu\n", of->ingressIf, dstAddr.numAddr());
		#endif 

		SPH::setDstAddr(packet, dstAddr);
		SPH::setCurrOFPtr(packet,SCION_ADDR_SIZE*2);
		
		//sendPacket(packet, outPacketLength, PORT_TO_SWITCH,TO_SERVER);
	}
}

int SCIONCertServer::verifyCert(uint8_t* packet){
	
	int ret = 0;
	uint16_t hops = 0;
	uint16_t hdrLen = SPH::getHdrLen(packet);
	certInfo* info = (certInfo*)(packet+hdrLen);
	uint64_t target = info->target;
	uint16_t length = info->length;
	

	unsigned char CertBuf[length];
	// Copy from packet
	memcpy(CertBuf, packet+hdrLen+CERT_INFO_SIZE, length);
	x509_crt TargetCert, TDCert;
	memset(&TDCert, 0, sizeof(x509_crt));
	memset(&TargetCert, 0, sizeof(x509_crt));

	// read target
	ret = x509_crt_parse(&TargetCert, CertBuf, length);
	if( ret < 0 ) {
	    #ifdef _DEBUG_CS
		scionPrinter->printLog(IH, (char*)"x509_crt_parse fails for target Cert.\n");
		#endif 
		x509_crt_free( &TargetCert );
		return SCION_FAILURE;
	}

	// read cert from ROT
	//SL: why read AD1? this needs to be changed.
	//read the list of core ADs and their certificates, 
	//keep it (possibly) in the memory, and verify verify ROT using them.
	ret = x509_crt_parse(&TDCert, (const unsigned char*)m_ROT.coreADs.find(1)->second.certificate, 
		m_ROT.coreADs.find(1)->second.certLen);
	
	if( ret < 0 ) {
	    #ifdef _DEBUG_CS
		scionPrinter->printLog(IH, (char*)"x509parse_crt fails for ROT cert.\n");
		#endif
		x509_crt_free( &TDCert );
		return SCION_FAILURE;
	}
	
	// verify using ROT cert
	int flag = 0;
	ret = x509_crt_verify(&TargetCert, &TDCert, NULL, NULL, &flag, NULL, NULL);
	if(ret!=0) {
	    #ifdef _DEBUG_CS
		scionPrinter->printLog(IH, (char*)"fail to verify received cert by ROT file.\n");
		#endif
		x509_crt_free(&TargetCert);
		x509_crt_free(&TDCert);
		return SCION_FAILURE;
	}

	x509_crt_free(&TargetCert);
	x509_crt_free(&TDCert);
	
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"save target cert.\n");
	#endif
	char cFileName[MAX_FILE_LEN];
	//SL: this part should be changed; parse path from config file
	sprintf(cFileName,"./TD1/Non-TDC/AD%llu/certserver/certificates/td%llu-ad%llu-0.crt", m_uAdAid, m_uTdAid, target);
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"Verify %s using ROT file.\n", cFileName);
	#endif

	FILE* cFile = fopen(cFileName,"w");
	fwrite(CertBuf, 1, length, cFile);
	fclose(cFile);

	return SCION_SUCCESS;
}

void SCIONCertServer::sendPacket(uint8_t* data, uint16_t data_length, string dest) {

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

void SCIONCertServer::getCertFile(uint8_t* fn, uint64_t target){
	//SL: this part should be changed; parse path from config file
	sprintf((char*)fn,"./TD1/Non-TDC/AD%llu/certserver/certificates/td%llu-ad%llu-0.crt",m_uAdAid, m_uTdAid, target);
}

void SCIONCertServer::reversePath(uint8_t* path, uint8_t* output, uint8_t hops){
	uint16_t offset = (hops)*OPAQUE_FIELD_SIZE; 
	uint8_t* ptr = path+OPAQUE_FIELD_SIZE;
	memcpy(output, path, OPAQUE_FIELD_SIZE);
	opaqueField* hopPtr = (opaqueField*)ptr; 
	
	for(int i=0;i<hops;i++){
		memcpy(output+offset, ptr, OPAQUE_FIELD_SIZE);
		offset-=OPAQUE_FIELD_SIZE;
		ptr+=OPAQUE_FIELD_SIZE;
		hopPtr = (opaqueField*)ptr;
	}
}

int SCIONCertServer::isCertRequested(uint64_t target, HostAddr requester){
	std::pair<std::multimap<uint64_t,
	HostAddr>::iterator,std::multimap<uint64_t,
        HostAddr>::iterator > range= m_certRequests.equal_range(target);
	
	std::multimap<uint64_t, HostAddr>::iterator itr;

	for(itr=range.first;itr!=range.second;itr++){
		if(itr->second == requester){
			return 1;
		}
	}
	return 0;
}

int SCIONCertServer::isROTRequested(uint32_t rotVer, HostAddr requester){
	std::pair<std::multimap<uint32_t, HostAddr>::iterator,
		std::multimap<uint32_t, HostAddr>::iterator > range 
		= m_ROTRequests.equal_range(rotVer);
	
	std::multimap<uint32_t, HostAddr>::iterator itr;

	for(itr=range.first;itr!=range.second;itr++){
		if(itr->second == requester){
			return ROT_REQ_SELF;
		}
	}
	if(range.first == range.second)
		return ROT_REQ_NO;
	else
		return ROT_REQ_OTHER;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONCertServer)


