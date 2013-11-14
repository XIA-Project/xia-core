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


/*change this to corresponding header*/
#include "scioncertserver.hh"


CLICK_DECLS

int SCIONCertServer::configure(Vector<String> &conf, ErrorHandler *errh){
	
	if(cp_va_kparse(conf, this, errh,
	"AID", cpkM, cpUnsigned64, &m_uAid,
	"CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
	"TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile, 
	"ROT", cpkM, cpString, &m_sROTFile,
	"Cert", cpkM, cpString, &m_csCert, // tempral store in click file
	"PrivateKey", cpkM, cpString, &m_csPrvKey, // tempral store in click file
	cpEnd) <0){
		printf("ERR: click configuration fail at SCIONCertServer.\n");
		printf("Exit SCON Network.\n");
		exit(-1);
	}
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

	// setup scionPrinter for message logging
	scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile);
	scionPrinter->printLog(IL, (char*)"CS INIT.\n");
	scionPrinter->printLog(IL, (char*)"ADAID = %llu, TDID = %llu.\n", m_uAdAid, m_uTdAid);

	// task 2: parse topology file
	parseTopology();
	constructIfid2AddrMap();
	initializeOutputPort();
	scionPrinter->printLog(IL, (char*)"Parse Topology Done.\n");
	
	// task 3: parse ROT (root of trust) file at local folder
	parseROT();
	scionPrinter->printLog(IL, (char*)"Parse and Verify ROT Done.\n");

	// task4: checking local cert and private key
	// TODO here
	
	ScheduleInfo::initialize_task(this, &_task, errh);
	return 0;
}

/*
	SCIONBeaconServer::initializeOutputPort
	prepare IP header for IP encapsulation
	if the port is assigned an IP address
*/
void SCIONCertServer::initializeOutputPort() {
	
	portInfo p;
	p.addr = m_Addr;
	m_vPortInfo.push_back(p);

	//Initialize port 0; i.e., prepare internal communication
	if(m_Addr.getType() == HOST_ADDR_IPV4) {
		m_pIPEncap = new SCIONIPEncap;
		m_pIPEncap->initialize(m_Addr.getIPv4Addr());
	}
}


/*
    parseROT
    parses ROT from m_sROTFile, which is defined as an argument in .click file
	if filename is given as an argument, parsing only verifies the (temporary) ROT file
	and returns the results.
*/
int SCIONCertServer::parseROT(String filename){	
	ROTParser parser;
	String fn;
	ROT tROT;
	
	if(filename.length())
		fn = filename;
	else
		fn = m_sROTFile;

	if(parser.loadROTFile(fn.c_str())!=ROTParseNoError){
		printf("ERR: ROT File missing at CS.\n");
		printf("fatal error, Exit SCION Network.\n");
		return SCION_FAILURE;
		//exit(-1);
	}
	scionPrinter->printLog(IL, "Load ROT OK.\n");

	if(parser.parse(tROT)!=ROTParseNoError){
		printf("ERR: ROT File parsing error at CS.\n");
		printf("Fatal error, Exit SCION Network.\n");
		return SCION_FAILURE;
		//exit(-1);
	}
	scionPrinter->printLog(IL, "Parse ROT OK.\n");

	if(parser.verifyROT(tROT)!=ROTParseNoError){
		printf("ERR: ROT File parsing error at CS.\n");
		printf("Exit SCION Network.\n");
		return SCION_FAILURE;
		//exit(-1);
	}

	//Store the ROT if verification passed.
	parser.parse(m_ROT);
	scionPrinter->printLog(IL, "Verify ROT OK.\n");

	// prepare ROT for delivery
	FILE* rotFile = fopen(fn.c_str(), "r");
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
	scionPrinter->printLog(IL, "CS: Stored Verified ROT for further delivery.\n");
	return SCION_SUCCESS;
}

void SCIONCertServer::parseTopology(){
	TopoParser parser;
	parser.loadTopoFile(m_sTopologyFile.c_str()); 
	parser.parseServers(m_servers);
	parser.parseRouters(m_routers);

	std::multimap<int, ServerElem>::iterator itr;
	for(itr = m_servers.begin(); itr != m_servers.end(); itr++)
		if(itr->second.aid == m_uAid){
			m_Addr = itr->second.addr;
			break;
		}
}

/*
	SLN:
	process ROT request, i.e., send ROT reply packet to BS
*/
void
SCIONCertServer::processLocalROTRequest(uint8_t * packet){
	#ifdef _SL_DEBUG_CS
	printf("CS (%llu:%llu): Received ROT request from BS (%llu).\n", 
		m_uAdAid, m_uAid, SPH::getSrcAddr(packet).numAddr());
	#endif

	uint8_t hdrLen = SPH::getHdrLen(packet);
	ROTRequest * req = (ROTRequest *)SPH::getData(packet);

	//1. return ROT  if the requested ROT is available from the local repository
	if(req->currentVersion == m_ROT.version) {

		scionHeader hdr;
	
		uint16_t totalLen = hdrLen + curROTLen;
		uint8_t rotPacket[totalLen];
		memset(rotPacket, 0, totalLen);
	
		// fill address
		hdr.src = m_Addr; //HostAddr(HOST_ADDR_SCION, m_uAid);
		hdr.dst = SPH::getSrcAddr(packet);
	
		hdr.cmn.type = ROT_REP_LOCAL;
		hdr.cmn.hdrLen = hdrLen;
		hdr.cmn.totalLen = totalLen;
	
		SPH::setHeader(rotPacket, hdr);
	
		memcpy(rotPacket+hdrLen, curROTRaw, curROTLen);
		sendPacket(rotPacket, totalLen, PORT_TO_SWITCH, TO_SERVER);
	//2. if not, request the ROT to the provider AD
	//note that the request contains the Opaque Field to reach the TDC
	} else {	
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
			sendPacket(packet, totalLength, PORT_TO_SWITCH, TO_ROUTER);
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
}

/*
	SLN:
	process ROT request from a child AD
*/
void
SCIONCertServer::processROTRequest(uint8_t * packet) {
	//1. return ROT if the requested version is available
	//2. forward it to the upstream otherwise (SL: I think this should not happen) 
	#ifdef _SL_DEBUG_CS
	printf("CS (%llu:%llu): Received ROT_REQ request from downstream CS.\n", m_uAdAid, m_uAid);
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
	sendPacket(buffer, hdr.cmn.totalLen, PORT_TO_SWITCH,TO_SERVER);
}

/*
	SLN:
	process certificate reply from an upstream AD
*/
void
SCIONCertServer::processROTReply(uint8_t * packet) {
	ROTRequest *req = (ROTRequest *)SPH::getData(packet);
	//SL:
	//check if the reply contains a higher ROT file
	//otherwise, don't propagate. Possibly, a bogus ROT request...
	//ToDo: should an error message be sent to Hosts and m_ROTRequesters cleared?
	if(req->previousVersion == req->currentVersion) {
		scionPrinter->printLog("The requested ROT does not exist.\n");
		m_ROTRequests.clear();
		return;
	}

	//1. verify if the returned ROT is correct
	//1.1. store as a temporary file
	char nFile[100];
	memset(nFile, 0, 100);
	sprintf(nFile,"./TD1/Non-TDC/AD%llu/certserver/ROT/rot-td%llu-%d.xml", m_uAdAid, m_uTdAid, req->currentVersion);

	String rotfn = String(nFile);
	printf("Update ROT file: %s\n", rotfn.c_str());
	
	uint16_t offset = sizeof(ROTRequest);
	SCL::saveToFile(packet, offset, SPH::getTotalLen(packet), rotfn);

	//1.2. verify using ROTParser
	if(parseROT(rotfn) == SCION_FAILURE) {
		//Invalid ROT is delivered.
		//Revert to the locally verified ROT
		parseROT();
		return;
	}
	
	// we do not need to back up again
	//1.3. backup the previous version and overwrite the received one to the current version of ROT.
	//String backupFile = m_sROTFile + String(".ver.") + String(req->previousVersion);
	//rename(m_sROTFile.c_str(), backupFile.c_str());
	//rename(rotfn.c_str(), m_sROTFile.c_str());
	m_sROTFile = rotfn;
	//Load the new ROT file
	//parseROT();

	//2. send it to the requester(s)
	std::pair<std::multimap<uint32_t,HostAddr>::iterator,std::multimap<uint32_t,HostAddr>::iterator > range
		= m_ROTRequests.equal_range(req->currentVersion);
	std::multimap<uint32_t, HostAddr>::iterator itr;
	
	for(itr=range.first;itr!=range.second;itr++){
		SPH::setType(packet, ROT_REP_LOCAL);
		SPH::setSrcAddr(packet, HostAddr(HOST_ADDR_SCION,m_uAid));
		SPH::setDstAddr(packet, itr->second);
		#ifdef _SL_DEBUG_CS
		printf("CS(%llu:%llu): ROT_REP_LOCAL to BS (%llu) \n",
			m_uAdAid, m_uAid, itr->second.numAddr());
		#endif

		sendPacket(packet, SPH::getTotalLen(packet), PORT_TO_SWITCH, TO_SERVER);
	}

	uint16_t pathLength = SPH::getHdrLen(packet)-COMMON_HEADER_SIZE;
	uint16_t currOFPtr = SPH::getCurrOFPtr(packet);
	if(currOFPtr+OPAQUE_FIELD_SIZE != pathLength){
		//SL: the following two lines are not necessary. will test this later...
		opaqueField* of = (opaqueField*)(packet+COMMON_HEADER_SIZE+currOFPtr);
		SPH::setDstAddr(packet, ifid2addr.find(of->egressIf)->second);

		sendPacket(packet, SPH::getTotalLen(packet), PORT_TO_SWITCH, TO_ROUTER);
	}
}
/*
	SLN:
	process certificate request from a child AD
*/
void
SCIONCertServer::processCertificateRequest(uint8_t * packet) {
	printf("CS (%llu:%llu): Received CERT_REQ request from downstream CS.\n", m_uAdAid, m_uAid);

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
			printf("CS (%llu:%llu): certificate not found, sending up stream.\n", m_uAdAid, target);
			
			newReq.targets[newReq.numTargets]=target;
			newReq.numTargets++;
		}else{
			printf("CS (%llu:%llu): certificate found sending down stream.\n", m_uAdAid, target);
			
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
			sendPacket(buffer, hdr.cmn.totalLen, PORT_TO_SWITCH,TO_SERVER);
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
		sendPacket(packet, outPacketLength, PORT_TO_SWITCH,TO_SERVER);
	}
}

/*
	SLN:
	process certificate reply from an upstream AD
*/
void
SCIONCertServer::processCertificateReply(uint8_t * packet) {
	printf("CS (%llu:%llu): Received CERT_REP request from upstream CS.\n", m_uAdAid, m_uAid);
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
		printf("CS(%llu:%llu): certificate verification failed.\n", m_uAdAid, m_uAid);
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
/*
	SLN:
	process certificate request from beacon server
	1. provide requested AD's certificate if available
	2. send a request to the upstream AD otherwise
*/
void
SCIONCertServer::processLocalCertificateRequest(uint8_t * packet) {
	HostAddr srcAddr = SPH::getSrcAddr(packet);
	HostAddr dstAddr = SPH::getDstAddr(packet);
	printf("CS (%llu:%llu): got CERT_REQ_LOCAL from BS (%llu)\n", m_uAdAid, m_uAid, srcAddr.numAddr());

	specialOpaqueField* sOF = (specialOpaqueField*)(packet+COMMON_HEADER_SIZE+srcAddr.getLength()+dstAddr.getLength());
	uint16_t hops = sOF->hops; 
	
	// get Cert Request Structure
	certReq* req = (certReq*)(packet+SPH::getHdrLen(packet));
	int numRequest = req->numTargets;
	certReq newReq = certReq();
	newReq.numTargets=0;

	for(int i=0;i<numRequest;i++){
		uint64_t target = req->targets[i];
		printf("Request target AID = %llu\n", target);
		uint8_t certFile[MAX_FILE_LEN];
		getCertFile(certFile, target);
		
		FILE* cFile;
		if((cFile=fopen((const char*)certFile,"r"))==NULL){
			printf("certificate not found, sending upstream\n"); 
			if(!isCertRequested(target, srcAddr)){
				m_certRequests.insert(std::pair<uint64_t, HostAddr>(target,srcAddr));
				newReq.targets[newReq.numTargets]=target;
				newReq.numTargets++;
				// go to label send upstream
			}

		}else{
			printf("CS (%llu:%llu): found AID %llu cert and send it back to BS (%llu)\n", 
				m_uAdAid, m_uAid, target, srcAddr.numAddr());
			fseek(cFile,0,SEEK_END);
			long cSize = ftell(cFile);
			rewind(cFile);
			
			scionHeader hdr;
			hdr.cmn.hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
			hdr.cmn.totalLen = hdr.cmn.hdrLen+CERT_INFO_SIZE+cSize;
			uint8_t certRepPkt[hdr.cmn.totalLen];
			
			hdr.cmn.type = CERT_REP_LOCAL;
			hdr.src = HostAddr(HOST_ADDR_SCION, m_uAid);
			hdr.dst = srcAddr;

			SPH::setHeader(certRepPkt,hdr);

			certInfo* info = (certInfo*)(certRepPkt+hdr.cmn.hdrLen);
			info->target= target;
			info->length = cSize;
			fread(certRepPkt+hdr.cmn.hdrLen+CERT_INFO_SIZE,1,cSize,cFile);
			fclose(cFile);
			sendPacket(certRepPkt, hdr.cmn.totalLen, PORT_TO_SWITCH,TO_SERVER);
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
		printf("ifid : %lu , destination : %llu\n", of->ingressIf, dstAddr.numAddr());

		SPH::setDstAddr(packet, dstAddr);
		SPH::setCurrOFPtr(packet,SCION_ADDR_SIZE*2);
		sendPacket(packet, outPacketLength, PORT_TO_SWITCH,TO_SERVER);
	}
}

/*
	read packets from the incoming NIC and process them based on their type
*/
bool SCIONCertServer::run_task(Task *task){

	Packet* inPacket;
	while((inPacket=input(PORT_TO_SWITCH).pull())){
		
		uint8_t * s_pkt = (uint8_t *) inPacket->data();
		//remove IP header if it exists
		if(m_vPortInfo[PORT_TO_SWITCH].addr.getType() == HOST_ADDR_IPV4){
			struct ip * p_iph = (struct ip *)s_pkt;
			struct udphdr * p_udph = (struct udphdr *)(p_iph+1);
			if(p_iph->ip_p != SCION_PROTO_NUM || ntohs(p_udph->dest) != SCION_PORT_NUM) {
				inPacket->kill();
				return true;
			}
			s_pkt += IPHDR_LEN;
		}

		uint16_t inPacketLength = SPH::getTotalLen((uint8_t*)s_pkt);
		uint8_t newPacket[inPacketLength];
		memcpy(newPacket, s_pkt, inPacketLength);
		inPacket->kill();
		uint16_t type = SPH::getType(newPacket);

		//SL:
		//the order needs to be changed
		//Certificate handling might be the most frequent event
		//Yet, switch-case can be used in CertServer
		switch(type) {
		case ROT_REQ:
			processROTRequest(newPacket); break;
		case ROT_REP:
			processROTReply(newPacket); break;
		case ROT_REQ_LOCAL:
			processLocalROTRequest(newPacket); break;
		case CERT_REQ: //send chain
			processCertificateRequest(newPacket); break;
		case CERT_REP:
			processCertificateReply(newPacket); break;
		case CERT_REQ_LOCAL:
			processLocalCertificateRequest(newPacket); break;
		case AID_REQ://aid req from switch
			#ifdef _SL_DEBUG_CS
			printf("CS (%llu): AID_REQ received...\n", m_uAdAid);
			#endif
			SPH::setType(newPacket, AID_REP);
			SPH::setSrcAddr(newPacket, HostAddr(HOST_ADDR_SCION, m_uAid));
			sendPacket(newPacket, inPacketLength, PORT_TO_SWITCH);
			break;
	default: 
			#ifdef _SL_DEBUG_CS
			printf("CS(%llu:%llu): unknown packet type: %d\n",m_uAdAid, m_uAid, type);
			#endif
			break;
		}
	}
	_task.fast_reschedule();
	return true;
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
	x509_cert TargetCert, TDCert;
	memset(&TDCert, 0, sizeof(x509_cert));
	memset(&TargetCert, 0, sizeof(x509_cert));

	// read target
	ret = x509parse_crt(&TargetCert, CertBuf, length);
	if( ret < 0 ) {
		printf("CS (%llu:%llu): x509parse_crt fails for target Cert.\n", m_uAdAid, m_uAid);
		x509_free( &TargetCert );
		return SCION_FAILURE;
	}

	// read cert from ROT
	//SL: why read AD1? this needs to be changed.
	//read the list of core ADs and their certificates, 
	//keep it (possibly) in the memory, and verify verify ROT using them.
	ret = x509parse_crt(&TDCert, (const unsigned char*)m_ROT.coreADs.find(1)->second.certificate, 
		m_ROT.coreADs.find(1)->second.certLen);
	
	if( ret < 0 ) {
		printf("CS (%llu:%llu): x509parse_crt fails for ROT cert.\n", m_uAdAid, m_uAid);
		x509_free( &TDCert );
		return SCION_FAILURE;
	}
	
	// verify using ROT cert
	int flag = 0;
	ret = x509parse_verify(&TargetCert, &TDCert, NULL, NULL, &flag, NULL, NULL);
	if(ret!=0) {
		printf("CS (%llu:%llu): fail to verify received cert by ROT file.\n", m_uAdAid, m_uAid);
		x509_free(&TargetCert);
		x509_free(&TDCert);
		return SCION_FAILURE;
	}

	x509_free(&TargetCert);
	x509_free(&TDCert);
	
	printf("save target cert.\n");
	char cFileName[MAX_FILE_LEN];
	//SL: this part should be changed; parse path from config file
	sprintf(cFileName,"./TD1/Non-TDC/AD%llu/certserver/certificates/td%llu-ad%llu-0.crt", m_uAdAid, m_uTdAid, target);
	printf("CS (%llu:%llu): Verify %s using ROT file.\n", m_uAdAid, m_uAid, cFileName);

	FILE* cFile = fopen(cFileName,"w");
	fwrite(CertBuf, 1, length, cFile);
	fclose(cFile);

	return SCION_SUCCESS;
}


void SCIONCertServer::sendPacket(uint8_t* data, uint16_t data_length, int port, int fwd_type){
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

	WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, data, data_length, DEFAULT_TL_ROOM);
	output(port).push(outPacket);
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

int SCIONCertServer::sendROT(uint32_t ROTVersion, HostAddr srcAddr){

	scionHeader hdr;

	hdr.cmn.type = ROT_REP_LOCAL;
	hdr.cmn.hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
	hdr.cmn.totalLen = curROTLen + hdr.cmn.hdrLen;
	hdr.src = HostAddr(HOST_ADDR_SCION, m_uAdAid);
	hdr.dst = srcAddr;

	uint8_t packet[hdr.cmn.totalLen];
	SPH::setHeader(packet,hdr);
	memcpy(packet+hdr.cmn.hdrLen, curROTRaw, curROTLen);
   
	sendPacket(packet, hdr.cmn.totalLen,PORT_TO_SWITCH);
	return 1;
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


void SCIONCertServer::constructIfid2AddrMap() {
    std::multimap<int, RouterElem>::iterator itr;
    for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
        ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interface.id, itr->second.addr));
    }
}


CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONCertServer)


