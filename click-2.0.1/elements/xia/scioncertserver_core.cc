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
#include "scioncertserver_core.hh"
#include "scionprint.hh"
#include "rot_parser.hh"

CLICK_DECLS

int SCIONCertServerCore::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh,
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile, 
        "TOPOLOGY_FILE",cpkM, cpString, &m_sTopologyFile,
        cpEnd) <0) {
            click_chatter("Fatal error: click configuration fail at SCIONCertServerCore.\n");
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

int SCIONCertServerCore::initialize(ErrorHandler* errh){

    // initialization task
    // task 1: parse config file
    Config config;
    config.parseConfigFile((char*)m_sConfigFile.c_str());

    // get ADAID, AID, TDID
    m_uAdAid = config.getAdAid();
    m_uTdAid = config.getTdAid();
    m_iLogLevel =config.getLogLevel();
    config.getCSLogFilename((char*)m_csLogFile);
    config.getPrivateKeyFilename((char*)m_csPrvKeyFile);
    config.getCertFilename((char*)m_csCertFile);
    config.getROTFilename((char*)m_sROTFile);

    // setup scionPrinter for logging
    scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile);
    #ifdef _DEBUG_CS
    scionPrinter->printLog(IH, (char*)"TDC CS (%s:%s) Initializes.\n", m_AD.c_str(), 
    m_HID.c_str());
    #endif

    // task 2: parse ROT (root of trust) file
    parseROT();
    #ifdef _DEBUG_CS
    scionPrinter->printLog(IH, (char*)"Parse/Verify ROT Done.\n");
    #endif

    // task 3: parse topology file
    parseTopology();
    #ifdef _DEBUG_CS
    scionPrinter->printLog(IH, (char*)"Parse Topology Done.\n");
    #endif

    #ifdef _DEBUG_CS
    scionPrinter->printLog(IH, (char*)"TDC CS (%s:%s) Initialization Done.\n", 
    m_AD.c_str(), m_HID.c_str());
    #endif
    
    // Trigger Timer
    _timer.initialize(this);
    //start after other elements are initialized
    _timer.schedule_after_sec(10);
    
    return 0;
}

void SCIONCertServerCore::run_timer(Timer *){
    sendHello();
    _timer.reschedule_after_sec(5);     // default speed
}

void SCIONCertServerCore::sendHello() {
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


void SCIONCertServerCore::parseROT(){
	ROTParser parser;
	if(parser.loadROTFile(m_sROTFile)!=ROTParseNoError){
    	click_chatter("Fatal error: ROT File missing at TDC CS.\n");
		exit(-1);
	}
	scionPrinter->printLog(IH, "Load ROT OK.\n");
	if(parser.parse(rot)!=ROTParseNoError){
    	click_chatter("Fatal error: ROT File parsing error at TDC CS.\n");
		exit(-1);
	}
	scionPrinter->printLog(IH, "Parse ROT OK.\n");
	if(parser.verifyROT(rot)!=ROTParseNoError) {
		click_chatter("Fatal error: ROT File verifying error at TDC CS.\n");
		exit(-1);
	}
	#ifdef _DEBUG_CS
	scionPrinter->printLog(IH, "Verify ROT OK.\n");
	#endif

	// prepare ROT for delivery
	FILE* rotFile = fopen(m_sROTFile, "r");
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
	scionPrinter->printLog(IH, (char*)"Stored Verified ROT for ROT Requests.\n");
	#endif
}

void SCIONCertServerCore::parseTopology(){
	TopoParser parser;
	parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
}

void SCIONCertServerCore::push(int port, Packet *p) {
    TransportHeader thdr(p);
    uint8_t *s_pkt = (uint8_t *) thdr.payload();
    uint16_t type = SPH::getType(s_pkt);
	uint16_t packetLength = SPH::getTotalLen(s_pkt);
    uint8_t packet[packetLength];

	memset(packet, 0, packetLength);
    memcpy(packet, s_pkt, packetLength);
    
    switch(type){
        case ROT_REQ_LOCAL: {
            XIAHeader xiahdr(p);
            #ifdef _DEBUG_CS
            scionPrinter->printLog(IH, (char *)"XIA src path = %s", xiahdr.src_path().unparse().c_str());
            scionPrinter->printLog(IH, (char *)"XIA dst path = %s", xiahdr.dst_path().unparse().c_str());
            #endif
            
            ROTRequest * req = (ROTRequest *)SPH::getData(packet);
            if(req->currentVersion == rot.version) {
                sendROT();
            }
		}
		break;

		case CERT_REQ: { //send chain

			uint16_t hops = 0;
			uint16_t hopPtr = 0;
			certReq* req= (certReq*)(packet+SCION_HEADER_SIZE+hops*PATH_HOP_SIZE);      
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
					reversePath(packet+SCION_HEADER_SIZE, downPath, hops);
					printf("TDC CS: certificate found sending down stream\n");
					fseek(cFile,0,SEEK_END);
					long cSize = ftell(cFile);
					rewind(cFile);

					uint16_t packetLength = SCION_HEADER_SIZE+CERT_INFO_SIZE+cSize+hops*PATH_HOP_SIZE;
					uint8_t buffer[packetLength];
					memcpy(buffer+SCION_HEADER_SIZE, downPath, hops*PATH_HOP_SIZE);
					SPH::setType(buffer, CERT_REP); 
					SPH::setTotalLen(buffer, packetLength);
					SPH::setDownpathFlag(buffer);
					pathHop *hop = (pathHop*)(buffer+SCION_HEADER_SIZE+(hops-hopPtr-1)*PATH_HOP_SIZE);
					certInfo* info = (certInfo*)(buffer+SCION_HEADER_SIZE+hops*PATH_HOP_SIZE);
					info->target= target;
					info->length = cSize;
					fread(buffer+SCION_HEADER_SIZE+CERT_INFO_SIZE+hops*PATH_HOP_SIZE,1,cSize,cFile);
					fclose(cFile);
					
					string dest = "RE ";
					dest.append(BHID);
					dest.append(" ");
					dest.append(m_AD.c_str());
					dest.append(" ");
					dest.append("HID:");
					dest.append((const char*)m_servers.find(BeaconServer)->second.HID);
					
					sendPacket(buffer, packetLength, dest);
					
				}
			}// end for
		} 
			break;
			
		case ROT_REQ:
			processROTRequest(packet);
			break;
			
		default:
			break;
		}
	p->kill();
}

void SCIONCertServerCore::processROTRequest(uint8_t * packet) {

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

	#ifdef _SL_DEBUG_CS
	printf("CS(%llu:%llu): ROT_REP: currOFPtr = %d, n_of = %d\n", 
		m_uAdAid, m_uAid, currOFPtr, hdr.n_of);
	#endif
	
	uint8_t buffer[hdr.cmn.totalLen];
	SPH::setHeader(buffer,hdr);
	SPH::setDownpathFlag(buffer);
	
	//3. copy ROT
	ROTRequest * req = (ROTRequest*)(buffer+hdr.cmn.hdrLen);
	//copy the ROT request hdr, telling the recipient the request version is being returned
	memcpy(req, SPH::getData(packet), sizeof(ROTRequest));	
	//If the requested ROT version is higher than the currently available one,
	//it indicates something wrong happened (i.e., unverified PCB propagated to a customer AD,
	//implying bogus PCB injection.
	//For now, we assume currentVersion = previousVersion +1. Otherwise, all versions of ROT
	//starting from previousVersion+1 to currentVersion should be provided to the requester.
	if(req->currentVersion != rot.version)
		req->currentVersion = rot.version;
	#ifdef _DEBUG_CS
	printf("CS (%llu:%llu): ROT_REP: req version = %d, send version = %d\n", 
		m_uAdAid, m_uAid, req->currentVersion, rot.version);
	#endif
	//copy the ROT
	memcpy(buffer+hdr.cmn.hdrLen+sizeof(ROTRequest), curROTRaw, curROTLen);	

	//4. Send ROT
	string dest = "RE ";
	dest.append(BHID);
	dest.append(" ");
	dest.append(m_AD.c_str());
	dest.append(" ");
	dest.append("HID:");
	dest.append((const char*)m_servers.find(BeaconServer)->second.HID);
					
	sendPacket(buffer, hdr.cmn.totalLen, dest);
}

void SCIONCertServerCore::sendPacket(uint8_t* data, uint16_t data_length, string dest) {

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

    q = xiah.encap(q, false);
	output(0).push(q);
}

void SCIONCertServerCore::getCertFile(uint8_t* fn, uint64_t target) {
	sprintf((char*)fn,"./TD1/TDC/AD%llu/certserver/certificates/td%llu-ad%llu-0.crt", 
	m_uAdAid, m_uTdAid, target);
}

void SCIONCertServerCore::reversePath(uint8_t* path, uint8_t* output, uint8_t hops) {
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

void SCIONCertServerCore::sendROT(){

    #ifdef _DEBUG_CS
	scionPrinter->printLog(IH, (char*)"TDC CS received ROT request from local BS.\n");
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
	    HostAddr srcAddr = HostAddr(HOST_ADDR_AIP, (uint8_t*)strchr(m_HID.c_str(),':')); 
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
		req.previousVersion = rot.version;
		req.currentVersion = rot.version; 
		*(ROTRequest *)(rotPacket+hdrLen) = req;
		
		memcpy(rotPacket+hdrLen+sizeof(struct ROTRequest), curROTRaw, curROTLen);
		sendPacket(rotPacket, totalLen, dest);
	}else{
	    #ifdef _DEBUG_CS
		scionPrinter->printLog(EH, (char*)"AD (%s) does not has beacon server.\n", m_AD.c_str());
		#endif
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONCertServerCore)


