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
#include "scioncertserver_core.hh"


CLICK_DECLS

int SCIONCertServerCore::configure(Vector<String> &conf, ErrorHandler *errh){
	
	if(cp_va_kparse(conf, this, errh,
	"AID", cpkM, cpUnsigned64, &m_uAid,
	"CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
	"TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile, 
	"ROT", cpkM, cpString, &m_sROTFile,
	"Cert", cpkM, cpString, &m_csCert, // tempral store in click file
	"PrivateKey", cpkM, cpString, &m_csPrvKey, // tempral store in click file
	cpEnd) <0) {
		printf("ERR: click configuration fail at SCIONCertServerCore.\n");
		printf("Fatal error, Exit SCION Network.\n");
		exit(-1);
	}
	return 0;
}

int SCIONCertServerCore::initialize(ErrorHandler* errh){
	
	// initialization task
	// task 1: parse config file
	Config config;
	config.parseConfigFile((char*)m_sConfigFile.c_str());
	// get ADAID, AID, TDID
	m_uAdAid = config.getAdAid();
	m_uTdAid = config.getTdAid();
	m_iLogLevel =config.getLogLevel();
	config.getCSLogFilename(m_csLogFile);

	// setup scionPrinter for message logging
	scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile);
	scionPrinter->printLog(IL, "TDC CS INIT.\n");
	scionPrinter->printLog(IL, "ADAID = %llu, TDID = %llu.\n", m_uAdAid, m_uTdAid);

	// task 2: parse ROT (root of trust) file
	parseROT();
	scionPrinter->printLog(IL, "Parse/Verify ROT Done.\n");
	
	// task 3: parse topology file
	parseTopology();
	scionPrinter->printLog(IL, "Parse Topology Done.\n");

	ScheduleInfo::initialize_task(this, &_task, errh);
	return 0;
}

void SCIONCertServerCore::parseROT(){
	ROTParser parser;
	if(parser.loadROTFile(m_sROTFile.c_str())!=ROTParseNoError){
    	printf("ERR: ROT File missing at TDC CS.\n");
		printf("Fatal error, Exit SCION Network.\n");
		exit(-1);
	}
	scionPrinter->printLog(IL, "Load ROT OK.\n");
	if(parser.parse(rot)!=ROTParseNoError){
    	printf("ERR: ROT File parsing error at TDC CS.\n");
		printf("Fatal error, Exit SCION Network.");
		exit(-1);
	}
	scionPrinter->printLog(IL, "Parse ROT OK.\n");
	if(parser.verifyROT(rot)!=ROTParseNoError) {
		printf("ERR: ROT File parsing error at TDC CS.\n");
		printf("Fatal error, Exit SCION Network.\n");
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
	scionPrinter->printLog(IL, "Stored Verified ROT for further delivery.\n");
}

void SCIONCertServerCore::parseTopology(){
	TopoParser parser;
	parser.loadTopoFile(m_sTopologyFile.c_str()); 
	parser.parseServers(m_servers);
	//SL: unused, hence commented out
	//routers may be required to distribute keys later...
	//parser.parseRouters(m_routers);
}

bool SCIONCertServerCore::run_task(Task *task){

	Packet* inPacket;

	while((inPacket=input(0).pull())){

	uint16_t inPacketLength =  SCIONPacketHeader::getTotalLen((uint8_t*)inPacket->data());
	uint8_t newPacket[inPacketLength];
	memcpy(newPacket, (uint8_t*)inPacket->data(), inPacketLength);
	inPacket->kill();
	uint16_t type = SCIONPacketHeader::getType(newPacket);

	if(type == ROT_REQ_LOCAL){

		cout << "TDC CS Core Received ROT request from TDC BS" << endl;
		cout << "Send ROT Response with ROT file to SCIONBeaconServerCore. Size =" << curROTLen << endl;
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

	}else if(type==AID_REQ){

		printf("TDC CS (%llu): AID_REQ received...\n", m_uAdAid);
		// bild AID_REP packet to sendback
		SCIONPacketHeader::setType(newPacket, AID_REP);
		HostAddr srcAddr(HOST_ADDR_SCION, m_uAid);
		SCIONPacketHeader::setSrcAddr(newPacket, srcAddr);
		sendPacket(newPacket, inPacketLength, PORT_TO_SWITCH);

	}else if(type==CERT_REQ){ //send chain

		uint16_t hops = 0;
		uint16_t hopPtr = 0;
		certReq* req= (certReq*)(newPacket+SCION_HEADER_SIZE+hops*PATH_HOP_SIZE);      
		certReq newReq = certReq();
		newReq.numTargets=0;

		for(int i=0;i<req->numTargets;i++){
			uint64_t target = req->targets[i];
			uint8_t certFile[MAX_FILE_LEN];
			printf("Cert Request Target = %llu in %llu\n", target, m_uAdAid);
			getCertFile(certFile, target);

			FILE* cFile;
			if((cFile=fopen((const char*)certFile,"r"))==NULL){
				if(certRequests.find(target)==certRequests.end()){
					printf("Fatal Error: TDC CS cannot found a Cert for CERT_REQ packet.\n");
					printf("It should exit the SCION network here.\n");
					exit(-1);
				}
			}else{
				uint8_t downPath[hops*PATH_HOP_SIZE];
				reversePath(newPacket+SCION_HEADER_SIZE, downPath, hops);
				printf("TDC CS: certificate found sending down stream\n");
				fseek(cFile,0,SEEK_END);
				long cSize = ftell(cFile);
				rewind(cFile);

				uint16_t packetLength = SCION_HEADER_SIZE+CERT_INFO_SIZE+cSize+hops*PATH_HOP_SIZE;
				uint8_t buffer[packetLength];
				memcpy(buffer+SCION_HEADER_SIZE,downPath,hops*PATH_HOP_SIZE);
				SCIONPacketHeader::setType(buffer, CERT_REP); 
				SCIONPacketHeader::setTotalLen(buffer, packetLength);
				pathHop *hop = (pathHop*)(buffer+SCION_HEADER_SIZE+(hops-hopPtr-1)*PATH_HOP_SIZE);
				certInfo* info = (certInfo*)(buffer+SCION_HEADER_SIZE+hops*PATH_HOP_SIZE);
				info->target= target;
				info->length = cSize;
				fread(buffer+SCION_HEADER_SIZE+CERT_INFO_SIZE+hops*PATH_HOP_SIZE,1,cSize,cFile);
				fclose(cFile);
				sendPacket(buffer, packetLength, PORT_TO_SWITCH);
			}
		}// end for

		/*
		// comment by Tenma, since TDC CS will never request any certs from upstream..

		if(newReq.numTargets!=0){
			uint16_t outPacketLength = SCIONPacketHeader::getTotalLen(newPacket);
			memcpy(newPacket+SCION_HEADER_SIZE+hops*PATH_HOP_SIZE,&newReq,CERT_REQ_SIZE);
			SCIONPacketHeader::setType(newPacket, CERT_REQ);
			uint8_t* ptr = newPacket+SCION_HEADER_SIZE;      
			uint16_t hopPtr = 0;
			pathHop* hop = (pathHop*)(ptr+PATH_HOP_SIZE*hopPtr);                
			uint16_t interface = hop->ingressIf;
			sendPacket(newPacket, outPacketLength, PORT_TO_SWITCH);
			//WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, newPacket, outPacketLength, DEFAULT_TL_ROOM);
			//output(0).push(outPacket);
		}
		*/
	}else if(type==CERT_REP){ 
		// remove by Tenma
		// will never reach here
		// no longer need, TDC CS hold all certs
        }else if(type==CERT_REQ_LOCAL){
		// remove by Tenma
		// TDC CS will never receive any CERT_REQ_LOCAL from TDC BS
	}else{
            printf("unknown packet type: cert server type=%d\n",type);
        }
    }
    _task.fast_reschedule();
    return true;
}


/*
int SCIONCertServerCore::verifyCert(uint8_t* packet){

	// TODO: verify certs
	uint16_t hops = 0;
	certInfo* info = (certInfo*)(packet+SCION_HEADER_SIZE+hops*PATH_HOP_SIZE);
	uint64_t target = info->target;
	uint16_t length = info->length;
	char cFileName[MAX_FILE_LEN];

	// should be fixed here since no td and version numbers
	// file format: td#-ad#-#.crt
	sprintf(cFileName,"./TD1/TDC/AD%llu/certserver/certificates/td1-ad%llu-0.crt",m_uAdAid,target);
	printf("%s\n",cFileName);
	FILE* cFile = fopen(cFileName,"w");
	fwrite(packet+SCION_HEADER_SIZE+CERT_INFO_SIZE+hops*PATH_HOP_SIZE,1,length,cFile);
	fclose(cFile);

	return SCION_SUCCESS;
}
*/

void SCIONCertServerCore::sendPacket(uint8_t* data, uint16_t dataLength, int port){
	//SLA:
	//IPv4 handling here
	//if addrtype == IPv4, encap with IP header
	//otherwise
	WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, data, dataLength,DEFAULT_TL_ROOM);
	output(port).push(outPacket);
}

void SCIONCertServerCore::getCertFile(uint8_t* fn, uint64_t target){
	sprintf((char*)fn,"./TD1/TDC/AD%llu/certserver/certificates/td%llu-ad%d-0.crt",m_uAdAid, m_uTdAid, target);
}

void SCIONCertServerCore::reversePath(uint8_t* path, uint8_t* output, uint8_t hops){
	uint16_t offset = (hops-1)*PATH_HOP_SIZE; 
	uint8_t* ptr = path;
	pathHop* hopPtr = (pathHop*)ptr; 
	for(int i=0;i<hops;i++){
		memcpy(output+offset, ptr, PATH_HOP_SIZE);
		offset-=PATH_HOP_SIZE;
		ptr+=PATH_HOP_SIZE;
		hopPtr = (pathHop*)ptr;
	}
}

void SCIONCertServerCore::sendROT(){
	uint16_t packetLen = curROTLen + SCION_HEADER_SIZE;
	uint8_t packet[packetLen];
	memcpy(packet+SCION_HEADER_SIZE,curROTRaw, curROTLen);
	SCIONPacketHeader::setType(packet, ROT_REP_LOCAL); 
	SCIONPacketHeader::setTotalLen(packet, packetLen);
	sendPacket(packet, packetLen,0);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONCertServerCore)


