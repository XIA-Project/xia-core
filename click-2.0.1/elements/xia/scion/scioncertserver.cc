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
    parseROT
    - parses ROT from m_sROTFile, which is defined as an argument in .click file
*/
void SCIONCertServer::parseROT(){	
	ROTParser parser;
	if(parser.loadROTFile(m_sROTFile.c_str())!=ROTParseNoError){
		printf("ERR: ROT File missing at CS.\n");
		printf("fatal error, Exit SCION Network.\n");
		exit(-1);
	}
	scionPrinter->printLog(IL, "Load ROT OK.\n");

	if(parser.parse(rot)!=ROTParseNoError){
		printf("ERR: ROT File parsing error at CS.\n");
		printf("Fatal error, Exit SCION Network.\n");
		exit(-1);
	}
	scionPrinter->printLog(IL, "Parse ROT OK.\n");

	if(parser.verifyROT(rot)!=ROTParseNoError){
		printf("ERR: ROT File parsing error at CS.\n");
		printf("Exit SCION Network.\n");
		exit(-1);
	}
	scionPrinter->printLog(IL, "Verify ROT OK.\n");
	// prepare ROT for delivery
	FILE* rotFile = fopen(m_sROTFile.c_str(), "r");
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
}

void SCIONCertServer::parseTopology(){
	TopoParser parser;
	parser.loadTopoFile(m_sTopologyFile.c_str()); 
	parser.parseServers(m_servers);
	parser.parseRouters(m_routers);
}

/*
	SLN:
	process ROT request, i.e., send ROT reply packet
*/
void
SCIONCertServer::processROTRequest(uint8_t * packet){
	printf("CS (%llu:%llu): Received ROT request from BS (%llu).\n", 
		m_uAdAid, m_uAid, SCIONPacketHeader::getSrcAddr(packet).numAddr());
	printf("\tSend ROT Response with ROT where size = %d.\n", curROTLen);
	
	uint8_t hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
	uint16_t totalLen = hdrLen + curROTLen;
	uint8_t rotPacket[totalLen];
	memset(rotPacket, 0, totalLen);
	// set length and type
	SCIONPacketHeader::setHdrLen(rotPacket, hdrLen);
	SCIONPacketHeader::setType(rotPacket, ROT_REP_LOCAL); 
	SCIONPacketHeader::setTotalLen(rotPacket, totalLen);
	// fill address
	HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
	HostAddr dstAddr = m_servers.find(BeaconServer)->second.addr;
	SCIONPacketHeader::setSrcAddr(rotPacket, srcAddr);
	SCIONPacketHeader::setDstAddr(rotPacket, dstAddr);
	memcpy(rotPacket+hdrLen, curROTRaw, curROTLen);
	sendPacket(rotPacket, totalLen, PORT_TO_SWITCH);
}

/*
	SLN:
	process certificate request from a child AD
*/
void
SCIONCertServer::processCertificateRequest(uint8_t * packet) {
	printf("CS (%llu:%llu): Received CERT_REQ request from downstream CS.\n", m_uAdAid, m_uAid);

	uint16_t hops = 0; 
	uint16_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
	specialOpaqueField* sOF =(specialOpaqueField*)(packet+SCION_ADDR_SIZE*2+COMMON_HEADER_SIZE);
	hops = sOF->hops;

	certReq* req= (certReq*)(packet+hdrLen);

	certReq newReq = certReq();
	newReq.numTargets=0;
	
	for(int i=0;i<req->numTargets;i++){
		uint64_t target = req->targets[i];
		uint8_t certFile[MAX_FILE_LEN];
		getCertFile(certFile, target);

		uint16_t currOFPtr = SCIONPacketHeader::getCurrOFPtr(packet);
		FILE* cFile;
		if((cFile=fopen((const char*)certFile,"r"))==NULL){
			printf("CS (%llu:%llu): certificate not found, sending up stream.\n", m_uAdAid, target);
			newReq.targets[newReq.numTargets]=target;
			newReq.numTargets++;
		}else{
			uint8_t downPath[(hops+1)*OPAQUE_FIELD_SIZE];
			reversePath(packet+COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2, downPath, hops);
			printf("CS (%llu:%llu): certificate found sending down stream.\n", m_uAdAid, target);
			fseek(cFile,0,SEEK_END);
			long cSize = ftell(cFile);
			rewind(cFile);
			uint16_t hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2+(hops+1)*OPAQUE_FIELD_SIZE;
			uint16_t packetLength = CERT_INFO_SIZE+cSize+hdrLen;
			uint8_t buffer[packetLength];
			memcpy(buffer+COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2 ,downPath,(hops+1)*OPAQUE_FIELD_SIZE);
			SCIONPacketHeader::setType(buffer, CERT_REP); 
			SCIONPacketHeader::setTotalLen(buffer, packetLength);
			SCIONPacketHeader::setHdrLen(buffer, hdrLen);

			SCIONPacketHeader::setTimestampPtr(buffer, COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2); 
			currOFPtr-=SCION_ADDR_SIZE*2;
			
			SCIONPacketHeader::setCurrOFPtr(buffer,(hops+1)*OPAQUE_FIELD_SIZE-currOFPtr+SCION_ADDR_SIZE*2);
			currOFPtr = SCIONPacketHeader::getCurrOFPtr(buffer);

			opaqueField* of = (opaqueField*)(downPath+currOFPtr-SCION_ADDR_SIZE*2);

			HostAddr dstAddr = ifid2addr.find(of->egressIf)->second;
			HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
			SCIONPacketHeader::setSrcAddr(buffer, srcAddr);
			SCIONPacketHeader::setDstAddr(buffer, dstAddr);
			
			certInfo* info = (certInfo*)(buffer+hdrLen);
			info->target= target;
			info->length = cSize;
			fread(buffer+hdrLen+CERT_INFO_SIZE,1,cSize,cFile);
			fclose(cFile);
			sendPacket(buffer, packetLength, PORT_TO_SWITCH);
		}
	} // end of for

	if(newReq.numTargets!=0){
		uint16_t outPacketLength =
		SCIONPacketHeader::getTotalLen(packet);
		memcpy(packet+hdrLen,&newReq,CERT_REQ_SIZE);
		SCIONPacketHeader::setType(packet, CERT_REQ);
		opaqueField* of = (opaqueField*)(packet+SCIONPacketHeader::getCurrOFPtr(packet));

		HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
		SCIONPacketHeader::setSrcAddr(packet, srcAddr);
		SCIONPacketHeader::setDstAddr(packet,ifid2addr.find(of->ingressIf)->second);
		sendPacket(packet, outPacketLength, PORT_TO_SWITCH);
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
		uint16_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
		certInfo* info = (certInfo*)(packet+hdrLen);
		uint64_t target = info->target;
		uint16_t length = info->length;
		// certRequests.erase(target); 
		std::pair<std::multimap<uint64_t,HostAddr>::iterator,std::multimap<uint64_t,HostAddr>::iterator > range
			= certRequests.equal_range(target);
		std::multimap<uint64_t, HostAddr>::iterator itr;
		for(itr=range.first;itr!=range.second;itr++){
			hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
			uint16_t bufferLength = hdrLen+CERT_INFO_SIZE+info->length; 
			uint8_t buffer[bufferLength];
			SCIONPacketHeader::setType(buffer, CERT_REP_LOCAL);
			info = (certInfo*)(buffer+hdrLen);
			info->target = target;
			info->length = length;
			memcpy(buffer+hdrLen+CERT_INFO_SIZE, packet+SCIONPacketHeader::getHdrLen(packet)+CERT_INFO_SIZE,length);
			SCIONPacketHeader::setTotalLen(buffer, bufferLength);
			SCIONPacketHeader::setHdrLen(buffer,hdrLen);
			HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, m_uAid);
			SCIONPacketHeader::setSrcAddr(buffer,srcAddr);
			SCIONPacketHeader::setDstAddr(buffer, itr->second);
			WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, buffer, bufferLength,DEFAULT_TL_ROOM);
			output(0).push(outPacket);
		}
	}else{
		printf("CS(%llu:%llu): certificate verification failed.\n", m_uAdAid, m_uAid);
	}

	uint16_t pathLength = SCIONPacketHeader::getHdrLen(packet)-COMMON_HEADER_SIZE;
	uint16_t currOFPtr = SCIONPacketHeader::getCurrOFPtr(packet);
	if(currOFPtr+OPAQUE_FIELD_SIZE != pathLength){
		opaqueField* of = (opaqueField*)(packet+COMMON_HEADER_SIZE+currOFPtr);
		SCIONPacketHeader::setDstAddr(packet, ifid2addr.find(of->egressIf)->second);
		WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM,
		packet,SCIONPacketHeader::getTotalLen(packet),DEFAULT_TL_ROOM);
		output(0).push(outPacket);
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
	HostAddr srcAddr = SCIONPacketHeader::getSrcAddr(packet);
	HostAddr dstAddr = SCIONPacketHeader::getDstAddr(packet);
	printf("CS (%llu:%llu): got CERT_REQ_LOCAL from BS (%llu)\n", m_uAdAid, m_uAid, srcAddr.numAddr());

	specialOpaqueField* sOF = (specialOpaqueField*)(packet+COMMON_HEADER_SIZE+srcAddr.getLength()+dstAddr.getLength());
	uint16_t hops = sOF->hops; 
	
	// get Cert Request Structure
	certReq* req = (certReq*)(packet+SCIONPacketHeader::getHdrLen(packet));
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
			HostAddr srcAddr =SCIONPacketHeader::getSrcAddr(packet);
			if(!isRequested(target, srcAddr)){
				certRequests.insert(std::pair<uint64_t, HostAddr>(target,srcAddr));
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

			uint16_t hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
			uint16_t packetLength = hdrLen+CERT_INFO_SIZE+cSize;
			uint8_t certRepPkt[packetLength];
			
			SCIONPacketHeader::setType(certRepPkt, CERT_REP_LOCAL);
			HostAddr homeAddr(HOST_ADDR_SCION, m_uAid);
			SCIONPacketHeader::setSrcAddr(certRepPkt, homeAddr);
			SCIONPacketHeader::setDstAddr(certRepPkt, srcAddr);
			SCIONPacketHeader::setHdrLen(certRepPkt, hdrLen);
			SCIONPacketHeader::setTotalLen(certRepPkt, packetLength);
			
			certInfo* info = (certInfo*)(certRepPkt+hdrLen);
			info->target= target;
			info->length = cSize;
			fread(certRepPkt+hdrLen+CERT_INFO_SIZE,1,cSize,cFile);
			fclose(cFile);
			sendPacket(certRepPkt, packetLength, PORT_TO_SWITCH);
		}
	}
	
	// lable::send upstream
	if(newReq.numTargets!=0){
		uint16_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
		uint16_t outPacketLength = SCIONPacketHeader::getTotalLen(packet);
		memcpy(packet+hdrLen,&newReq,CERT_REQ_SIZE);
		SCIONPacketHeader::setType(packet, CERT_REQ);

		opaqueField* of = (opaqueField*)(packet+COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2+OPAQUE_FIELD_SIZE);
		HostAddr dstAddr = ifid2addr.find(of->ingressIf)->second;     
		printf("ifid : %lu , destination : %llu\n", of->ingressIf, dstAddr.numAddr());

		SCIONPacketHeader::setDstAddr(packet, dstAddr);
		SCIONPacketHeader::setCurrOFPtr(packet,SCION_ADDR_SIZE*2);
		sendPacket(packet, outPacketLength, PORT_TO_SWITCH);
	}
}

/*
	read packets from the incoming NIC and process them based on their type
*/
bool SCIONCertServer::run_task(Task *task){

	Packet* inPacket;
	while((inPacket=input(0).pull())){
		uint16_t inPacketLength = SCIONPacketHeader::getTotalLen((uint8_t*)inPacket->data());
		uint8_t newPacket[inPacketLength];
		memcpy(newPacket, (uint8_t*)inPacket->data(), inPacketLength);
		inPacket->kill();
		uint16_t type = SCIONPacketHeader::getType(newPacket);

		//SL:
		//the order needs to be changed
		//Certificate handling might be the most frequent event
		//Yet, switch-case can be used in CertServer

		if(type == ROT_REQ){
			// TODO
			// For ROT version changes
			//SL: note - this routine is not for handling version change
		}else if(type == ROT_REQ_LOCAL){

			processROTRequest(newPacket);

			//aid req from switch
		}else if(type==AID_REQ){
			
			// AID_REQ, reply AID_REP packet to switch	
			printf("CS (%llu): AID_REQ received...\n", m_uAdAid);
			SCIONPacketHeader::setType(newPacket, AID_REP);
			HostAddr srcAddr(HOST_ADDR_SCION, m_uAid);
			SCIONPacketHeader::setSrcAddr(newPacket, srcAddr);
			sendPacket(newPacket, inPacketLength, PORT_TO_SWITCH);
			//_AIDIsRegister = true;

		}else if(type==CERT_REQ){ //send chain
			
			processCertificateRequest(newPacket);

		}else if(type==CERT_REP){ 
			
			processCertificateReply(newPacket);

		}else if(type==CERT_REQ_LOCAL){
			
			processLocalCertificateRequest(newPacket);

		}else{
			printf("unknown packet type: cert server type=%d\n",type);
		}
	}
	_task.fast_reschedule();
	return true;
}

int SCIONCertServer::verifyCert(uint8_t* packet){
	
	int ret = 0;
	uint16_t hops = 0;
	uint16_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
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
	ret = x509parse_crt(&TDCert, (const unsigned char*)rot.coreADs.find(1)->second.certificate, rot.coreADs.find(1)->second.certLen);
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
	sprintf(cFileName,"./TD1/Non-TDC/AD%llu/certserver/certificates/td%llu-ad%llu-0.crt", m_uAdAid, m_uTdAid, target);
	printf("CS (%llu:%llu): Verify %s using ROT file.\n", m_uAdAid, m_uAid, cFileName);

	FILE* cFile = fopen(cFileName,"w");
	fwrite(CertBuf, 1, length, cFile);
	fclose(cFile);

	return SCION_SUCCESS;
}


void SCIONCertServer::sendPacket(uint8_t* data, uint16_t dataLength, int port){
	// printf("CS (%llu): %dth Port: sending packet (%dB)\n", m_uAdAid,port, dataLength);
	//SLA:
	//IPv4 handling here
	//if addrtype == IPv4, encap with IP header
	//otherwise
	WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, data, dataLength, DEFAULT_TL_ROOM);
	output(port).push(outPacket);
}

void SCIONCertServer::getCertFile(uint8_t* fn, uint64_t target){
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

	uint16_t hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
	uint16_t packetLen = curROTLen + hdrLen;
	uint8_t packet[packetLen];
	memcpy(packet+hdrLen, curROTRaw, curROTLen);
   
	SCIONPacketHeader::setType(packet, ROT_REP_LOCAL); 
	SCIONPacketHeader::setTotalLen(packet, packetLen);
	SCIONPacketHeader::setHdrLen(packet, hdrLen);
	SCIONPacketHeader::setDstAddr(packet, srcAddr);
	sendPacket(packet, packetLen,0);
	return 1;
}

int SCIONCertServer::isRequested(uint64_t target, HostAddr requester){
	std::pair<std::multimap<uint64_t,
	HostAddr>::iterator,std::multimap<uint64_t,
        HostAddr>::iterator > range= certRequests.equal_range(target);
	
	std::multimap<uint64_t, HostAddr>::iterator itr;

	for(itr=range.first;itr!=range.second;itr++){
		if(itr->second == requester){
			return 1;
		}
	}
	return 0;
}


void SCIONCertServer::constructIfid2AddrMap() {
    std::multimap<int, RouterElem>::iterator itr;
    for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
        ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interfaceID, itr->second.addr));
    }
}


CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONCertServer)


