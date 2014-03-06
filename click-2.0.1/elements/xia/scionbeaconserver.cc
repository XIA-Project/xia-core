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
#include <polarssl/sha1.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/*change this to corresponding header*/
#include "scionbeaconserver.hh"
#include "scioncommonlib.hh"
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#include <click/xiaheader.hh>
#include <click/xiacontentheader.hh>
#include "xiaxidroutetable.hh"
#define SID_XROUTE "SID:1110000000000000000000000000000000001112"
#define MYAD "AD:1000000000000000000000000000000000000001"
#define MYHID "HID:0000000000000000000000000000000000000001"
#include <click/xiacontentheader.hh>
#include "xiatransport.hh"
#include "xtransport.hh"
#include <click/xiatransportheader.hh>

CLICK_DECLS

/*
    SCIONBeaconServer::configure
    read configuration file, topology file, and Root of Trust (RoT) file .
*/
int SCIONBeaconServer::configure(Vector<String> &conf, ErrorHandler *errh){
	if(cp_va_kparse(conf, this, errh, 
	"AID", cpkM, cpUnsigned64, &m_uAid,
	"CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
	"TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile,
	"ROT", cpkM, cpString, &m_sROTFile, // Tenma, tempral exist but should merge into config class
	cpEnd) <0){
		printf("ERR: click configuration fail at BS.\n");
		printf("Fatal error, Exit SCION Network.\n");
		exit(-1);
	}
	return 0;
}
/*
    SCIONBeaconServer:;initialize
    - click function to initialize click variables
*/

int SCIONBeaconServer::initialize(ErrorHandler* errh){

#ifdef ENABLE_AESNI
	if(!check_for_aes_instructions()) {
		printf("scion beacon server did not support AESNI instructions.\n");
		exit(-1);
	}
#endif

	// TODO: put it in config file
	m_iRecheckTime=4;

	Config config;
	config.parseConfigFile((char*)m_sConfigFile.c_str());

	m_uAdAid = config.getAdAid();
	m_uTdAid = config.getTdAid();
	m_iRegTime = config.getRTime();
	m_iPropTime = config.getPTime();
	
	m_iLogLevel =config.getLogLevel();

	config.getPrivateKeyFilename((char*)m_csPrvKeyFile);
	config.getCertFilename((char*)m_csCertFile);
	config.getOfgmKey((char*)m_uMkey);
	config.getPCBLogFilename(m_csLogFile);
    
	//SL: what is this? Not used anywhere
	m_iResetTime = config.getResetTime();
	m_iScheduleTime = SCIONCommonLib::GCD(m_iRegTime, m_iPropTime);
	printf("PCB reg = %d, prop = %d, schedule time = %d\n",m_iRegTime, m_iPropTime, m_iScheduleTime);
	m_iIsRegister = config.getIsRegister(); //whether to register paths to TDC or not
	m_iKval = config.getNumRegisterPath(); //# of paths that can be registered to TDC
	m_iBeaconTableSize = config.getPCBQueueSize();

	time(&m_lastPropTime);
	m_lastRegTime = m_lastPropTime;

	scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile);
	scionPrinter->printLog(IL, (char*)"BS %llu INIT.\n", m_uAid);
	scionPrinter->printLog(IL, (char*)"ADAID = %llu, TDID = %llu.\n", m_uAdAid, m_uTdAid);

	// task 2: parse ROT (root of trust) file
	m_bROTInitiated = parseROT();
	scionPrinter->printLog(IL, (char*)"Parse and Verify ROT Done.\n");

	// task 3: parse topology file
	parseTopology();
	constructIfid2AddrMap();
	initializeOutputPort();
	scionPrinter->printLog(IL, (char*)"Parse Topology Done.\n");

	// task 4: read key pair if they already exist
	loadPrivateKey();
	scionPrinter->printLog(IL, (char*)"PrivateKey Loading Successful.\n");

	// task 5: generate/update OFG key
	initOfgKey();
	updateOfgKey();
	srand(time(NULL));

	//HC: initialize per-child path selection policy
	initSelectionPolicy();

  // Task 6: Initialize XIA addresses
    // make the dest DAG (broadcast to other routers)
    m_ddag = (char*)malloc(snprintf(NULL, 0, "RE %s %s", BHID, SID_XROUTE) + 1);
    sprintf(m_ddag, "RE %s %s", BHID, SID_XROUTE);	

    // make the src DAG (the one the routing process listens on)
    m_sdag = (char*) malloc(snprintf(NULL, 0, "RE %s %s %s", MYAD, MYHID, SID_XROUTE) + 1);
    sprintf(m_sdag, "RE %s %s %s", MYAD, MYHID, SID_XROUTE); 

	ScheduleInfo::initialize_task(this, &_task, errh);
	_timer.initialize(this); 
	_timer.schedule_after_sec(m_iScheduleTime);

	return 0;
}

/*
	SCIONBeaconServer::initializeOutputPort
	prepare IP header for IP encapsulation
	if the port is assigned an IP address
*/
void SCIONBeaconServer::initializeOutputPort() {
	
	portInfo p;
	p.addr = m_Addr;
	m_vPortInfo.push_back(p);

	//Initialize port 0; i.e., prepare internal communication
	if(m_Addr.getType() == HOST_ADDR_IPV4) {
		m_pIPEncap = new SCIONIPEncap;
		m_pIPEncap->initialize(m_Addr.getIPv4Addr());
	}
}


/*SL: Initialize OFG key */
bool SCIONBeaconServer::initOfgKey() {

	time_t currTime;
	time(&currTime);
	//SL: to deal with the case that TDC BS initiate PCB before this BS starts
	currTime -= 300; //back 5minutes
	m_currOfgKey.time = currTime;
	memcpy(m_currOfgKey.key, &m_uMkey, OFG_KEY_SIZE);

#ifdef ENABLE_AESNI
	if(AESKeyExpand((UCHAR*)m_currOfgKey.key, &m_currOfgKey.aesnikey, OFG_KEY_SIZE_BITS)==NULL)
	{
		printf("Enc Key setup failure by AESKeyExpand\n");
		return SCION_FAILURE;
	}
#else
	int err;
	if(err = aes_setkey_enc(&m_currOfgKey.actx, m_currOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}
#endif

	return SCION_SUCCESS;
}

/*SL: Update OFG key when a new key is available from CS*/
bool SCIONBeaconServer::updateOfgKey() {
	//This function should be updated...
	m_prevOfgKey.time = m_currOfgKey.time;
	memcpy(m_prevOfgKey.key, m_currOfgKey.key, OFG_KEY_SIZE);

#ifdef ENABLE_AESNI
	if(AESKeyExpand((UCHAR*)m_prevOfgKey.key, &m_prevOfgKey.aesnikey, OFG_KEY_SIZE_BITS)==NULL)
	{
		printf("Enc Key setup failure by AESNI KeyExpand function.\n");
		return SCION_FAILURE;
	}
#else
	int err;
	if(err = aes_setkey_enc(&m_prevOfgKey.actx, m_prevOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Prev. Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}
#endif

	return SCION_SUCCESS;
}

/*
    SCIONBeaconServer:;parseTopology
    - Parse topology.xml file and put values into 
      structures (servers, routers, gateways)
	  currently gateways is missing
*/
void 
SCIONBeaconServer::parseTopology(){
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

/* SLT:
	Construct ifid2addr map
*/
void SCIONBeaconServer::constructIfid2AddrMap() {
	std::multimap<int, RouterElem>::iterator itr;
	for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
		ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interface.id, itr->second.addr));
	}
}


void SCIONBeaconServer::loadPrivateKey() {
	if(!_CryptoIsReady) {
		rsa_init(&PriKey, RSA_PKCS_V15, 0);
		int err = x509parse_keyfile(&PriKey, m_csPrvKeyFile, NULL);
		if(err) {
			rsa_free(&PriKey);
			printf("ERR: Private key file loading failure, path = %s\n", (char*)m_csPrvKeyFile);
			printf("Fatal error, Exit SCION Network.\n");
			exit(-1);
		}
		// check private key
		if(rsa_check_pubkey(&PriKey)!=0)
		{
			rsa_free(&PriKey);
			printf("ERR: Private key is not well-formatted, path = %s\n", (char*)m_csPrvKeyFile);
			printf("Fatal error, Exit SCION Network.\n");
			exit(-1);
		}	
		_CryptoIsReady = true;
	}  
}


//Load RoT file and verify it.
bool SCIONBeaconServer::parseROT(String filename){
	ROTParser parser;
	String fn;
	if(filename.length())
                fn = filename;
        else
                fn = m_sROTFile;
	
	if(parser.loadROTFile(fn.c_str())!=ROTParseNoError) {
		scionPrinter->printLog(IL, (char*)"ERR: ROT File missing at BS %llu.\n", m_uAdAid);
		return false;
	}else{
		// ROT found in local folder
    	if(parser.parse(m_cROT)!=ROTParseNoError){
			scionPrinter->printLog(IL, (char*)"ERR: ROT File parsing error at BS %llu.\n", m_uAdAid);
			return false;
		}
		if(parser.verifyROT(m_cROT)!=ROTParseNoError){
			scionPrinter->printLog(IL, (char*)"ERR: ROT File verification error at BS %llu.\n", m_uAdAid);
			return false;
		}
	}
	return true;
}

/*
	SLN:
	Send an ROT request to the (local) certificate server
*/

void
SCIONBeaconServer::requestROT() {
	m_bROTRequested = true;

	uint8_t hdrLen = COMMON_HEADER_SIZE+DEFAULT_ADDR_SIZE*2;
	uint16_t totalLen = hdrLen + ROT_VERSION_SIZE;
	uint8_t packet[totalLen];

	scionHeader hdr;
	hdr.cmn.type = ROT_REQ_LOCAL;
	hdr.cmn.hdrLen = hdrLen;
	hdr.cmn.totalLen = totalLen;

	hdr.src = HostAddr(HOST_ADDR_SCION, m_uAid);
	hdr.dst = m_servers.find(CertificateServer)->second.addr;

	SPH::setHeader(packet, hdr);

	// version number, try to get 0
	//SL: Here, we need to provide the ROT version number to fetch
	//this request needs to handle many versions simultaneously since BS may 
	//have outdated version in its local directory
	//E.g., the most recent version that is locally available is 1, 
	//yet the current version in PCB is 10. This can happen if BS is turned on
	//after having been off for a long while.
	//However, since all certificates are managed/verified by CS, BS only requests
	//the current version of ROT to CS. 
	struct ROTRequest req;
	req.previousVersion = 0;
	req.currentVersion = 0; //currentVersion == 0 indicates the most recent version
							//since BS does not have any ROT available
	*(ROTRequest *)(packet+hdrLen) = req;
	sendPacket(packet, totalLen, PORT_TO_SWITCH, TO_SERVER);
}

/*
    SCIONBeaconServer::run_timer
    - runs every m_iScheduleTime seconds.
      1. Propagates pcb every m_iProptime
      2. Registers pcb to TDC Path Server every m_iRegTime
*/
void 
SCIONBeaconServer::run_timer(Timer *timer){
    sendHello();
#if 0

	time_t curTime;
	time(&curTime);
	//SL: either use multiple timers or reduce the reschedule interval
	//currently it's GCD of Reg and Prop Times
	
	// check if ROT file is missing
	if(!m_bROTInitiated)
	{
		scionPrinter->printLog(IH,"BS (%llu:%llu): ROT is missing or wrong formatted.\n", m_uAdAid, m_uAid);
		
		// send an ROT request (i.e., ROT_REQ_LOCAL) if its AID is registered to SCION switch
		if(_AIDIsRegister && !m_bROTRequested) 
			requestROT();
	}
	
	// time for registration
	if(m_iIsRegister && curTime - m_lastRegTime >= m_iRegTime){
		registerPaths();
		time(&m_lastRegTime); //updates the time
	}
	
	// check ROT and Crypto are ready
	// time for propagation
	if(curTime - m_lastPropTime >= m_iPropTime){
		propagate();
		time(&m_lastPropTime); //updates the time
	}
	
	// recheck unverified PCBs (due to missing certificates)...
	if(curTime-m_lastRecheckTime >= m_iRecheckTime){
		recheckPcb(); 
	}
#endif

	_timer.reschedule_after_sec(m_iScheduleTime);
}

/*
	SLN:
	Send an ROT request to the (local) certificate server
	when the ROT version is changed
	note: calling requestROT may be sufficient for most of the case
	since CS of the provider AD should be available most of the time
*/

void
SCIONBeaconServer::requestROT(SPacket * packet, uint32_t rotVer) {
	//1. Reset the ROTInitiated flag
	m_bROTInitiated = false;
	m_bROTRequested = true;


	//2. build UP path to the TDC
	//this is intended to request ROT if the CS of the provider AD is not available
	//path contains special opaque field (i.e., timestamp)
#ifdef ENABLE_AESNI
	keystruct ks;
	getOfgKey(SPH::getTimestamp(packet),ks);
#else
	aes_context actx;
	getOfgKey(SPH::getTimestamp(packet),actx);
#endif
	
	uint16_t tmpPacketLength = SPH::getTotalLen(packet);
	tmpPacketLength += PCB_MARKING_SIZE; //reserve space for an additional marking
	uint8_t tmpPacket[tmpPacketLength];  //for copying the original packet to the increased buffer
	memcpy(tmpPacket,packet,tmpPacketLength-PCB_MARKING_SIZE);

	uint16_t ingIf = SCIONBeaconLib::getInterface(packet); 
	
#ifdef ENABLE_AESNI
	SCIONBeaconLib::addLink(tmpPacket, ingIf, NON_PCB, 1, m_uAdAid, m_uTdAid, &ks, 0, 0, 0, (uint16_t)PriKey.len);
#else
	SCIONBeaconLib::addLink(tmpPacket, ingIf, NON_PCB, 1, m_uAdAid, m_uTdAid, &actx, 0, 0, 0, (uint16_t)PriKey.len);
#endif

	uint8_t hops = SCIONBeaconLib::getNumHops(packet);
	hops = hops +2;
	uint8_t path[OPAQUE_FIELD_SIZE*hops]; 		//+2 counts the current AD's marking
												//note: current AD hop has not been increased
												//while the ingress router put its marking
	buildPath(tmpPacket, path);

	//SLT:
	//4. Set Header (i.e., common header and src/dst addresses)
	//for CERT_REQ packet, src/dst addresses are SCION type (i.e., 8B)
	//4.1 Common Header
	uint8_t offset = SCION_ADDR_SIZE *2;
	uint8_t hdrLen = COMMON_HEADER_SIZE+offset+OPAQUE_FIELD_SIZE*hops;

	uint16_t packetLength = hdrLen+sizeof(ROTRequest);
	uint8_t newPacket[packetLength];

	scionHeader hdr;

	hdr.src = HostAddr(HOST_ADDR_SCION, m_uAid);
	hdr.dst = m_servers.find(CertificateServer)->second.addr;

	hdr.cmn.type = ROT_REQ_LOCAL;
	hdr.cmn.hdrLen = hdrLen;
	hdr.cmn.totalLen = packetLength;
	hdr.cmn.currOF = offset;
	hdr.cmn.timestamp = offset;
	hdr.cmn.flag |= UP_PATH_FLAG;

	//4.2 Set opaque fields 
	hdr.n_of = hops;
	hdr.p_of = path;
	//set the number of hops in the timestamp OF
	specialOpaqueField* sOF = (specialOpaqueField *)path;
	sOF->hops = hops -1;

	SPH::setHeader(newPacket, hdr);

	struct ROTRequest req;
	req.previousVersion = m_cROT.version;
	req.currentVersion = rotVer;
	*(ROTRequest*)(newPacket+hdrLen) = req;
	sendPacket(newPacket, packetLength, PORT_TO_SWITCH, TO_SERVER);
}

/*
	SLN:
	process a new PCB
*/

void
SCIONBeaconServer::processPCB(SPacket * packet, uint16_t packetLength){

	uint8_t srcLen = SPH::getSrcLen(packet);
	uint8_t dstLen = SPH::getDstLen(packet);
	uint32_t ts = SPH::getTimestamp(packet);
	HostAddr srcAddr = SPH::getSrcAddr(packet);
	uint8_t type = BEACON;

	//SLN:
	scionPrinter->printLog(IH,type,ts,srcAddr.numAddr(),m_uAdAid,"%u,RECEIVED\n",packetLength);
	
	//PCB verification
	if(verifyPcb(packet)!=SCION_SUCCESS){
		scionPrinter->printLog(WH,type,ts,srcAddr.numAddr(),m_uAdAid,"%u,VERIFICATION FAILED\n", packetLength);
	}else{
		scionPrinter->printLog(IL,type,ts,srcAddr.numAddr(),m_uAdAid,"%u,VERIFICATION PASSED\n" ,packetLength);
		//adds pcb to beacon table
		addPcb(packet);

		//send pcb to all path servers (for up-paths). 
		//SL: better to make a different ft (since code is too long)
		//SL: send it to all path servers (not just one... ) 
		
		//SL: Expiration time needs to be configured by AD's policy
		uint8_t exp = 0; //expiration time

		if(m_servers.find(PathServer)!=m_servers.end()){
			std::multimap<int, ServerElem>::iterator it;
			std::pair<std::multimap<int, ServerElem>::iterator,std::multimap<int,ServerElem>::iterator> pathServerRange;
			pathServerRange = m_servers.equal_range(PathServer);
			
#ifdef ENABLE_AESNI
			keystruct ks;
			if(getOfgKey(SPH::getTimestamp(packet),ks)) {
				//SL: OFG key retrieval failure
				scionPrinter->printLog(EH,type,ts,srcAddr.numAddr(),m_uAdAid,"OFGKEY retrieval failure\n");
				return;
			}
#else
			aes_context  actx;
			if(getOfgKey(SPH::getTimestamp(packet),actx)) {
				//SL: OFG key retrieval failure
				scionPrinter->printLog(EH,type,ts,srcAddr.numAddr(),m_uAdAid,"OFGKEY retrieval failure\n");
				return;
			}
#endif

			uint16_t ingress = SCIONBeaconLib::getInterface(packet);
			packetLength += PCB_MARKING_SIZE;

			//adds own marking to pcb
			uint8_t newPacket[packetLength];
			memset(newPacket, 0, packetLength);
			memcpy(newPacket, packet, packetLength-PCB_MARKING_SIZE);

#ifdef ENABLE_AESNI
			SCIONBeaconLib::addLink(newPacket, ingress, NON_PCB, 1, m_uAdAid, m_uTdAid, &ks, 0,exp,0, (uint16_t)PriKey.len);
#else
			SCIONBeaconLib::addLink(newPacket, ingress, NON_PCB, 1, m_uAdAid, m_uTdAid, &actx, 0,exp,0, (uint16_t)PriKey.len);
#endif

			//removes signature from pcb
			uint8_t path[packetLength]; 
			uint16_t pathLength = removeSignature(newPacket, path);
			uint8_t hdrLen = COMMON_HEADER_SIZE+srcLen+dstLen;
			
			memset(newPacket+hdrLen,0,packetLength-hdrLen);
			memcpy(newPacket+hdrLen,path, pathLength);
			
			//TODO:Addr	
			scionHeader hdr;
			hdr.cmn.type = UP_PATH;
			hdr.cmn.hdrLen = hdrLen;
			hdr.cmn.totalLen = hdrLen+pathLength;

			hdr.src = HostAddr(HOST_ADDR_SCION, m_uAdAid);
			
			//SL: send this packet to all path servers -- done
			for(it=pathServerRange.first; it!=pathServerRange.second; it++) { 
				hdr.dst = it->second.addr;
				SPH::setHeader(newPacket, hdr);
				sendPacket(newPacket, packetLength, PORT_TO_SWITCH, TO_SERVER);
			}
		} else {
			#ifdef _SL_DEBUG_BS
			printf("AD%llu: no path server exists\n", m_uAdAid);
			#endif
		}
	}
}

/*
	SLN:
	Save certificate to a local repository
	SL: fragmentation may need to be considered later.
*/
void
SCIONBeaconServer::saveCertificate(SPacket * packet, uint16_t packetLength) {
	uint8_t hdrLen = SPH::getHdrLen(packet);
	certInfo* info = (certInfo*)(packet+hdrLen);
	uint64_t target = info->target;
	uint16_t cLength = info->length;
	
	char cFileName[MAX_FILE_LEN];
	//SL: this should be changed; e.g., parsing from the config file path. 
	sprintf(cFileName,"./TD1/Non-TDC/AD%llu/beaconserver/certificates/td%llu-ad%llu-0.crt", m_uAdAid, m_uTdAid, target);
	// erase from the request queue
	certRequest.erase(target);
	// write it!
	FILE* cFile = fopen(cFileName,"w");
	
	fwrite(packet+hdrLen+CERT_INFO_SIZE,1,cLength,cFile);
	fclose(cFile);
}
	
/*
	SLN:
	update ifid_map
*/
void
SCIONBeaconServer::updateIfidMap(SPacket * packet) {
	uint32_t ts = 0;
	uint64_t src = 0;
				
	uint8_t hdrLen = SPH::getHdrLen(packet);
	IFIDRequest * ir = (IFIDRequest *)(packet+hdrLen);
	uint16_t nifid = ir->reply_id;  
	uint16_t ifid = ir->request_id;
	ifid_map.insert(pair<uint16_t, uint16_t>(ifid, nifid));

	scionPrinter->printLog(IH, SPH::getType(packet),"BS (%llu:%llu): IFID received (neighbor:%d - self:%d)\n", m_uAdAid,m_uAid, nifid, ifid);
	//SL:
	//print information needs to be revised everywhere...
}


/*
	SLN:
	saveROT file to local repository
	SL: ROT may not fit into a single file
	Fragmentation (-> reassembly) needs to be considered immediately
*/
void
SCIONBeaconServer::saveROT(SPacket * packet, uint16_t packetLength)
{

	ROTRequest *req = (ROTRequest *)SPH::getData(packet);
	int curver = (int)req->currentVersion;
	scionPrinter->printLog(IL, SPH::getType(packet), "BS (%llu:%llu): ROT Replay cV = %d, pV = %llu\n", m_uAdAid, m_uAid, curver, m_cROT.version);

	if(curver>m_cROT.version) {
	
       //1. verify if the returned ROT is correct
	   char nFile[100];
       memset(nFile, 0, 100);
	   sprintf(nFile,"./TD1/Non-TDC/AD%llu/beaconserver/ROT/rot-td%llu-%d.xml", m_uAdAid, m_uTdAid, curver);

       String rotfn = String(nFile);
	   scionPrinter->printLog(IL, "BS (%llu:%llu): Update ROT file as %s\n", m_uAdAid, m_uAid, nFile);

		int offset = SPH::getHdrLen(packet)+sizeof(ROTRequest);
		int rotLen = packetLength-offset;
			
		// Write to file defined in Config
		FILE* rotFile = fopen(rotfn.c_str(), "w+");
		fwrite(packet+offset, 1, rotLen, rotFile);
		fclose(rotFile);

       	//1.2. verify using ROTParser
		m_bROTInitiated = parseROT(rotfn);
        if(m_bROTInitiated == SCION_FAILURE) {
	    	scionPrinter->printLog(IL, "BS (%llu:%llu) fail to parse ROT.\n", m_uAdAid, m_uAid);
	    	// remove file
	    	remove(nFile);
	    	return;
        }else{
	    	m_sROTFile = rotfn;
	    	scionPrinter->printLog(IL, "BS (%llu:%llu) stored received ROT.\n", m_uAdAid, m_uAid);
	    	m_bROTRequested = false;
		}
	}
}

/*
	SLN:
	send AID reply packet to SCION switch
*/
void
SCIONBeaconServer::sendAIDReply(SPacket * packet, uint16_t packetLength){
	/*AID_REQ : AID request from switch, sends back its AID*/
	//SL: changed w/ the new packet format...
	scionHeader hdr;
	
	//set src/dst addresses
	hdr.src = HostAddr(HOST_ADDR_SCION,m_uAid);

	//set common header
	hdr.cmn.type = AID_REP;
	hdr.cmn.hdrLen = COMMON_HEADER_SIZE+hdr.src.getLength();
	hdr.cmn.totalLen = packetLength;
	
	SPH::setHeader(packet,hdr);

	uint16_t type = SPH::getType(packet);
	time_t ts = time(NULL);

	//SLT: temporarily blocked...
	scionPrinter->printLog(IH,type,ts,hdr.src.numAddr(),m_uAdAid,"AID REQ: %u,RECEIVED\n",packetLength);

	sendPacket(packet, packetLength, PORT_TO_SWITCH); 
	_AIDIsRegister = true;
}

void SCIONBeaconServer::push(int port, Packet *p)
{
    TransportHeader thdr(p);
    /*
    //Pulls packet from queue until the queue is empty.
    Packet* inPacket;
    while((inPacket = input(PORT_TO_SWITCH).pull())){
        
        //extract and copy packet data to 'packet' to avoid memory sharing problem
        //and kills packet
		//SL: this problem may disappear if queue is implemented...
       	
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
        */


		uint8_t * s_pkt = (uint8_t *) thdr.payload();
        s_pkt += 2;

        uint16_t type = SPH::getType(s_pkt);
		uint16_t packetLength = SPH::getTotalLen(s_pkt);
        uint8_t packet[packetLength];

		//SL: unnecessary?
		memset(packet, 0, packetLength);
        memcpy(packet, s_pkt, packetLength);
        //inPacket->kill();

        p->kill();
	
		if(type==BEACON){
			//PCB arrived        
			/*BEACON: verifies signature
			- if passes then add to beacon table, remove signature, and send to Path Server
			- if not, ignores. 
			*/
			if(!m_bROTInitiated){
				scionPrinter->printLog(IH, "BS (%llu:%llu): No valid ROT file! Ignoring PCB.\n", m_uAdAid, m_uAid);
				#ifdef _SL_DEBUG_BS
				printf("BS (%llu:%llu): No valid ROT file! Ignoring PCB.\n", m_uAdAid, m_uAid);
				#endif
				//continue;
                return;
			}
			
			uint32_t ROTVersion = SCIONBeaconLib::getROTver(packet);
			#ifdef _SL_DEBUG_BS
			printf("BS (%llu:%llu): Recived RoT version = %d.\n", m_uAdAid, m_uAid, ROTVersion);
			printf("BS (%llu:%llu): Self RoT version = %d.\n", m_uAdAid, m_uAid, m_cROT.version);
			#endif
			//SL: ROT version is changed...
            if(ROTVersion > m_cROT.version){
				#ifdef _SL_DEBUG_BS
				printf("BS (%llu:%llu): RoT version has been changed. Get a new ROT from local CS.\n", m_uAdAid, m_uAid);
				#endif
				requestROT(packet,ROTVersion);
				//continue;
				return;
			}// ROT version change handler


			processPCB(packet,packetLength);

		}else {
			switch(type) {
			case CERT_REP_LOCAL:
				saveCertificate(packet,packetLength);
				break;

			case IFID_REP:
				/*
				IFID_REP : maps router IFID with neighboring IFID
				Only active interface ID will be inserted in the ifid_map.
				The interfaces without any mappings but still in the topology file
				will not be added (or propagated ) to the beacon.
				*/
				//IFIDNEW
				//SL: Border routers should send IFID_REP as soon as neighbors' IFID is available.
				//This should be implemented at routers

				updateIfidMap(packet);
				break;

			case AID_REQ:
				sendAIDReply(packet,packetLength);
				break;

			case ROT_REP_LOCAL:
				#ifdef _SL_DEBUG_BS
				printf("BS (%llu:%llu): Received ROT_REP_LOCAL packets from local CS.\n", m_uAdAid, m_uAid);
				#endif
				saveROT(packet,packetLength);
				break;
			default:
				//scionPrinter->printLog(IH, "BS(%llu:%llu) Unsupported type (%d) packet.\n",m_uAdAid,m_uAid,type);
				break;
			}
		}
    //}// end of while
}

/*
    SCIONBeaconServer:: run_task
    - The main routine for PCB handling.
*/
bool 
SCIONBeaconServer::run_task(Task *task){
#if 0
	
    //Pulls packet from queue until the queue is empty.
    Packet* inPacket;
    while((inPacket = input(PORT_TO_SWITCH).pull())){
        
        //extract and copy packet data to 'packet' to avoid memory sharing problem
        //and kills packet
		//SL: this problem may disappear if queue is implemented...
       	
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

        uint16_t type = SPH::getType(s_pkt);
		uint16_t packetLength = SPH::getTotalLen(s_pkt);
        uint8_t packet[packetLength];
        
		//SL: unnecessary?
		memset(packet, 0, packetLength);
        memcpy(packet, s_pkt, packetLength);
        inPacket->kill();
	
		if(type==BEACON){
			//PCB arrived        
			/*BEACON: verifies signature
			- if passes then add to beacon table, remove signature, and send to Path Server
			- if not, ignores. 
			*/
			if(!m_bROTInitiated){
				scionPrinter->printLog(IH, "BS (%llu:%llu): No valid ROT file! Ignoring PCB.\n", m_uAdAid, m_uAid);
				#ifdef _SL_DEBUG_BS
				printf("BS (%llu:%llu): No valid ROT file! Ignoring PCB.\n", m_uAdAid, m_uAid);
				#endif
				continue;
			}
			
			uint32_t ROTVersion = SCIONBeaconLib::getROTver(packet);
			#ifdef _SL_DEBUG_BS
			printf("BS (%llu:%llu): Recived RoT version = %d.\n", m_uAdAid, m_uAid, ROTVersion);
			printf("BS (%llu:%llu): Self RoT version = %d.\n", m_uAdAid, m_uAid, m_cROT.version);
			#endif
			//SL: ROT version is changed...
            if(ROTVersion > m_cROT.version){
				#ifdef _SL_DEBUG_BS
				printf("BS (%llu:%llu): RoT version has been changed. Get a new ROT from local CS.\n", m_uAdAid, m_uAid);
				#endif
				requestROT(packet,ROTVersion);
				continue;
			}// ROT version change handler

			processPCB(packet,packetLength);

		}else {
			switch(type) {
			case CERT_REP_LOCAL:
				saveCertificate(packet,packetLength);
				break;

			case IFID_REP:
				/*
				IFID_REP : maps router IFID with neighboring IFID
				Only active interface ID will be inserted in the ifid_map.
				The interfaces without any mappings but still in the topology file
				will not be added (or propagated ) to the beacon.
				*/
				//IFIDNEW
				//SL: Border routers should send IFID_REP as soon as neighbors' IFID is available.
				//This should be implemented at routers

				updateIfidMap(packet);
				break;

			case AID_REQ:
				sendAIDReply(packet,packetLength);
				break;

			case ROT_REP_LOCAL:
				#ifdef _SL_DEBUG_BS
				printf("BS (%llu:%llu): Received ROT_REP_LOCAL packets from local CS.\n", m_uAdAid, m_uAid);
				#endif
				saveROT(packet,packetLength);
				break;
			default:
				scionPrinter->printLog(IH, "BS(%llu:%llu) Unsupported type (%d) packet.\n",m_uAdAid,m_uAid,type);
				break;
			}
		}
    }// end of while
#endif

    _task.fast_reschedule();
    return true;
}

/*
    SCIONBeaconServer::removeSignature
    - Removes all the signatures in the PCB
    - Returns the size of the PCB w/o the signature. 
*/
uint16_t 
SCIONBeaconServer::removeSignature(SPacket* inPacket, uint8_t* path){

	uint16_t totalLength = SPH::getTotalLen(inPacket);
	uint16_t numHop = SCIONBeaconLib::getNumHops(inPacket);
	uint8_t buf[totalLength];
	memset(buf, 0, totalLength);
	uint8_t hdrLen = SPH::getHdrLen(inPacket);

	//SLT: buf has special OF + series of OFs
	//1. copy the special opaque field (TS) to the buffer.
	memcpy(buf, inPacket+hdrLen, OPAQUE_FIELD_SIZE);

	uint8_t* ptr = inPacket+hdrLen+OPAQUE_FIELD_SIZE*2;
	pcbMarking* mrkPtr = (pcbMarking*)ptr;
	uint16_t offset = OPAQUE_FIELD_SIZE;

	//2. copy markings excluding signature
	for(int i=0;i<numHop;i++){
		uint16_t blkSize = mrkPtr->blkSize;

		memcpy(buf+offset, ptr, blkSize);
		ptr+= (blkSize+mrkPtr->sigLen);
		mrkPtr=(pcbMarking*)ptr;
		offset+=blkSize;
	}
	memcpy(path, buf, offset);
	return offset;
}

/*
    SCIONBeaconServer::createHash
    - Creates 'scionHash' a unique ID for pcb.
*/

scionHash 
SCIONBeaconServer::createHash(SPacket* pkt){
	//SLT: not used....
}

/*
    SCIONBeaconServer::addPcb
    -adds verified pcb to the beacon table. 
*/

void SCIONBeaconServer::addPcb(SPacket* pkt) {
	//if the beacon table is full then remove the oldest pcb
	//SL: the first element is the oldest since multimap sort elements by the key (i.e., timestamp)...
	//Or, we can remove already propagated paths...
	if(beacon_table.size() >= m_iBeaconTableSize){
		free(beacon_table.begin()->second.msg);
		beacon_table.erase(beacon_table.begin());
	}

	uint16_t pktLen = SPH::getTotalLen(pkt);
	uint32_t ts = SPH::getTimestamp(pkt);

	// initialize the new pcb 
	// copy everything back
	pcb newPcb = pcb();   
	newPcb.totalLength = pktLen;
	newPcb.hops = SCIONBeaconLib::getNumHops(pkt);
	newPcb.timestamp = ts;
	newPcb.msg = (uint8_t*)malloc(pktLen); //stores the entire pcb
	newPcb.propagated=0;
	newPcb.registered=0;
	newPcb.ingress = SCIONBeaconLib::getInterface(pkt);

	memset(newPcb.msg, 0, pktLen);
	memcpy(newPcb.msg, pkt, pktLen);
    
	//SL: multimap orders elements automatically
	//need to consider how to optimize insertion using iterator (i.e., position)
	std::pair<uint32_t, pcb> npair = std::pair<uint32_t,pcb>(ts, newPcb);
	beacon_table.insert(npair);
}

void SCIONBeaconServer::addUnverifiedPcb(SPacket* pkt){

	uint16_t pktLen = SPH::getTotalLen(pkt);
	uint32_t ts = SPH::getTimestamp(pkt);

	if(unverified_pcbs.size() > MAX_UNVERIFIED_PCB)
		return;
    
	//initialize the new pcb 
	//beacon is stored in BS as a form of pcb struct
	//msg in the struct is the original PCB from its parent
	pcb newPcb = pcb();
	newPcb.totalLength = pktLen;
	newPcb.hops = SCIONBeaconLib::getNumHops(pkt);
	newPcb.timestamp = ts; 
	newPcb.msg = (uint8_t*)malloc(pktLen); //stores the entire pcb
	newPcb.propagated=0;
	newPcb.registered=0;
	newPcb.ingress = SCIONBeaconLib::getInterface(pkt);

	memset(newPcb.msg, 0, pktLen);
	memcpy(newPcb.msg, pkt, pktLen);

	std::pair<uint32_t, pcb> npair = std::pair<uint32_t,pcb>(ts, newPcb);
	unverified_pcbs.insert(npair);
}

/*
    SCIONBeaconServer::registerPaths
    - iterates through beacon table and seek for paths to register. 
    - If fully registered, delete 3 paths and register 3 new paths. 

    NOTE: This part of the code will be replaced with HC's code OR
    I will re-implement it with HC's path selection model.
*/

int 
SCIONBeaconServer::registerPaths(){
    //printf("BS: registering path to TDC, AD%d: Beacon Table Size =%d\n", m_uAdAid, beacon_table.size());
	//SL: indicator showing the process is running
    fprintf(stderr,".");

    if(beacon_table.empty()){
        return 0;
    }
    
    /*
        If it is already fully registered. 
    */
     
    if(m_iNumRegisteredPath>= m_iKval){ //fully registered
		//if all k paths are already registered, removes 3 registered pcb (just for testing)
		//SLT: need to check path expiration time
		//just for test ---
		//if k paths are already registered, the BS replaces only the oldest one with a new fresh path
		//path replacement policy needs to be defined in more detail
        for(int i=0;i<3;i++){  
            k_paths.erase(k_paths.begin());
            m_iNumRegisteredPath--;
        }
    
    }
    
    
    //register up to k paths from the newest one in the beacon table 
	//Note: registered paths are not thrown away in order to compute "path fidelity."
    // if k paths are fully registered, then stop.
    
	//reverse iterator is better to find a best (fresh) path since the key is ordered by timestamp in the map
	std::multimap<uint32_t, pcb>::reverse_iterator itr;
	
	for(int i = m_iNumRegisteredPath; i < m_iKval; i++) {
		if(getPathToRegister(itr)){
			#ifdef _SL_DEBUG_BS
			printf("\nAD%llu: %i paths registered to TDC. No more path left to register\n", m_uAdAid,i-1);
			#endif
    		scionPrinter->printLog(IH,PATH_REG, "AD%llu: %i paths registered to TDC. No path to register\n",m_uAdAid,i-1);
			break;
		}
		
		registerPath(itr->second);
        
		//post handling of registered paths
		itr->second.registered=1;
        m_iNumRegisteredPath++;
        k_paths.insert(std::pair<uint32_t, pcb>(itr->first, itr->second));
		//SL: what if path registration failed at TDC for some unknown reason?
		//need to handle this as a special case in the future (consistency between BS and TDC PS)
	}
    return 0;
}

////////////////////////////////////////////////////////////
//SL: Pick a best path to register to TDC
//Currently newest path is the best path to register
//This should be replaced with a more appropriate policy
////////////////////////////////////////////////////////////
bool
SCIONBeaconServer::getPathToRegister(std::multimap<uint32_t, pcb>::reverse_iterator &iter) {
    
    std::multimap<uint32_t, pcb>::reverse_iterator itr;

	for(itr = beacon_table.rbegin();itr!=beacon_table.rend();itr++){
		//Simple probabilistic selection (set higher probability to more recent paths)
        if(!itr->second.registered && (rand()%2)){
			iter = itr;
            return SCION_SUCCESS;
        }
    }

	return SCION_FAILURE;
}

////////////////////////////////////////////////////////////////
//SL: This part is unused currently
////////////////////////////////////////////////////////////////
void 
SCIONBeaconServer::printPaths(){
    
    std::multimap<uint32_t, pcb>::iterator itr; 
    
    for(itr=beacon_table.begin();itr!=beacon_table.end();itr++){
        uint8_t hdrLen = SPH::getHdrLen(itr->second.msg); 
        uint8_t* ptr = itr->second.msg+hdrLen+OPAQUE_FIELD_SIZE*2;
        pcbMarking* mrkPtr = (pcbMarking*)ptr;
        for(int i=0;i<itr->second.hops;i++){
            printf("%llu(%d:%d) | ", mrkPtr->aid,  mrkPtr->ingressIf, mrkPtr->egressIf);
            ptr+=mrkPtr->sigLen + mrkPtr->blkSize;
            mrkPtr = (pcbMarking*)ptr;
        }
    
        printf("%llu  %d\n",m_uAdAid, itr->second.registered);
    
    }

}
////////////////////////////////////////////////////////////////

/*
    SCIONBeaconServer::registerPath
    - Send register packet to TDC path server. 
    - Parameter :
        rpcb : path to be regitsered (selected from registerPaths() )
*/

int 
SCIONBeaconServer::registerPath(pcb &rpcb){

	//SLT:ref
	//1. construct a path by copying pcb to packet
	uint16_t packetLength = rpcb.totalLength+PCB_MARKING_SIZE;
	uint8_t packet[packetLength];
	memset(packet,0,packetLength);

	//SLT:ref
	//2. original pcb received from the upstream
	memcpy(packet,rpcb.msg,rpcb.totalLength);

#ifdef ENABLE_AESNI
	keystruct ks;
	getOfgKey(SPH::getTimestamp(packet),ks);
#else
	aes_context actx;
	getOfgKey(SPH::getTimestamp(packet),actx);
#endif  

	//3. add its own marking information to the end
	//SL: Expiration time needs to be configured by AD's policy
	uint8_t exp = 0; //expiration time
	uint16_t sigLen = PriKey.len;

#ifdef ENABLE_AESNI
	SCIONBeaconLib::addLink(packet, rpcb.ingress, NON_PCB, 1, m_uAdAid, m_uTdAid, &ks,0,exp,0, sigLen);
#else
	SCIONBeaconLib::addLink(packet, rpcb.ingress, NON_PCB, 1, m_uAdAid, m_uTdAid, &actx,0,exp,0, sigLen);
#endif

	//4. Remove signature from PCB
	//pathContent would get special OF (TS) (without 2nd special OF) + series of markings from TDC to itself
	//this would be registered to PS in TDC
	uint8_t pathContent[packetLength];
	memset(pathContent,0,packetLength);
	uint16_t pathContentLength = removeSignature(packet, pathContent);
    
	// Build path (list of OF) from the pcb
	//SLT: add 1 for special OF (i.e., timestamp)
	//5. Now construct a path to the TDC (i.e., a series of OFs to the TDC)
	uint16_t numHop = SCIONBeaconLib::getNumHops(packet)+1; //added 1 for the special OF (i.e., TS)
	uint8_t path[numHop*OPAQUE_FIELD_SIZE];
	buildPath(packet, path);
    
	//6. at this point we have "path" (TS+OFs to TDC) and "pathContent" that would be registered to TDC PS.
	uint8_t srcLen = SPH::getSrcLen(packet);
	uint8_t dstLen = SPH::getDstLen(packet);

	//7. now hdr for path registration include COMMON_HDR + SRC/DST Addr + TS + OFs
	uint8_t hdrLen = COMMON_HEADER_SIZE+srcLen+dstLen+numHop*OPAQUE_FIELD_SIZE;

	//create new packet
	uint16_t newPacketLength = hdrLen+pathContentLength;
	uint8_t newPacket[newPacketLength];
	uint16_t interface = ((opaqueField*)(path+OPAQUE_FIELD_SIZE))->ingressIf;

	//////////////////////////////////////////////////////////////////////////
	// SL:
	// A new way to set packet header...
	// Fill the scionHeader structure and call setHeader
	// instead of calling individual field as previously done.
	scionHeader hdr;
	//set common header
	hdr.cmn.type = PATH_REG;
	hdr.cmn.hdrLen = hdrLen;
	hdr.cmn.totalLen = newPacketLength;
	hdr.cmn.currOF = srcLen+dstLen;
	hdr.cmn.numOF = numHop; //* not a mandatory property for path registration.

	//set src/dst addresses
	hdr.src = HostAddr(HOST_ADDR_SCION, m_uAdAid);
	//SL: set destination addr to that of the router holding the interface id.
	hdr.dst = ifid2addr.find(interface)->second;

	//set opaque fields
	hdr.n_of = numHop;
	hdr.p_of = path;
	
	//write to the packet
	SPH::setHeader(newPacket, hdr);
	////////////////////////////////////////////////////////////////////////////

	//SLT: 
	//Here, we need to set special OF to help routers to forward packets
	specialOpaqueField pSOF = *(specialOpaqueField *)path;

	//SLT: this is already marked by TDC BS when it initiated PCB propagation
	//Just confirm this is the packet to TDC
	pSOF.info = 0x80;
	SPH::setTimestampOF(newPacket, pSOF);
	SPH::setUppathFlag(newPacket);
	memcpy(newPacket+hdrLen,pathContent,pathContentLength);
    
	char buf[MAXLINELEN];
	uint16_t offset = 0;
	uint8_t* ptr = pathContent+OPAQUE_FIELD_SIZE;
	pcbMarking* mrkPtr = (pcbMarking*)ptr;

	for(int i=0;i<numHop;i++){
		sprintf(buf+offset,"%llu (%u, %u) |", mrkPtr->aid, mrkPtr->ingressIf, mrkPtr->egressIf);
		ptr+=mrkPtr->blkSize;
		mrkPtr=(pcbMarking*)ptr;
		offset = strlen(buf);
	}
     
	scionPrinter->printLog(IH,PATH_REG,rpcb.timestamp,
        m_uAdAid,0,"%u,SENT PATH: %s\n",newPacketLength,buf);
	sendPacket(newPacket, newPacketLength, PORT_TO_SWITCH, TO_ROUTER);
	
	return 0;
}

/*
    SCIONBeaconServer::buildPath (list of OF)
    Builds path to TDC using the current PCB;
	i.e., staring from the current AD, ending with TDC
	Note: common header and src/dst addresses must be set by the caller
*/
int SCIONBeaconServer::buildPath(SPacket* pkt, uint8_t* output){
	
	
	//SL: need to make pkt type "const"
	//to do this, all SPH::getXXX need to be changed
	uint8_t hdrLen = SPH::getHdrLen(pkt);

	//SLT:
	//1. special Opaque Field (TS)
	memcpy(output, pkt+hdrLen,OPAQUE_FIELD_SIZE);

	uint8_t* ptr = pkt + hdrLen+OPAQUE_FIELD_SIZE*2; //after 2 special OFs, AD marking comes.
	pcbMarking* mrkPtr = (pcbMarking*)ptr;
	uint16_t hops = SCIONBeaconLib::getNumHops(pkt);
	//2. place the pointer to the end of the marking so as to construct a reverse path
	uint16_t offset = (hops)*OPAQUE_FIELD_SIZE;
    
	//3. now iterate from itself to TDC and construct a series of OFs.
	for(int i=0;i<hops;i++){
		opaqueField newHop = opaqueField(0x00, mrkPtr->ingressIf, mrkPtr->egressIf, 0, mrkPtr->mac);
		memcpy(output+offset, &newHop, OPAQUE_FIELD_SIZE);
		ptr+=mrkPtr->blkSize + mrkPtr->sigLen;
		mrkPtr = (pcbMarking*)ptr;
		offset-=OPAQUE_FIELD_SIZE;
	}
	return 0;
}

void SCIONBeaconServer::print(SPacket* path, int hops){
	uint8_t* ptr = path;
	opaqueField* pathPtr = (opaqueField*)(ptr+OPAQUE_FIELD_SIZE);
	for(int i=0;i<hops;i++){
		printf("ingress=%u egress=%u\n",pathPtr->ingressIf,
		pathPtr->egressIf);
		ptr += OPAQUE_FIELD_SIZE;
		pathPtr = (opaqueField*)ptr;
	}
}


int SCIONBeaconServer::propagate(){

	// Nothing should propagate 
	if(beacon_table.empty()) return 0;

	std::multimap<uint32_t, pcb>::iterator itr;
	std::multimap<int, RouterElem>::iterator pItr; //iterator for peers
	std::multimap<int, RouterElem>::iterator cItr; //iterator for child routers

	// find all the peer routers
	std::pair<std::multimap<int, RouterElem>::iterator,
	std::multimap<int,RouterElem>::iterator> peerRange;
	peerRange = m_routers.equal_range(Peer);

	//find all the child routers
	std::pair<std::multimap<int, RouterElem>::iterator,
	std::multimap<int,RouterElem>::iterator> childRange;
	childRange = m_routers.equal_range(Child);

	//SL: Expiration time needs to be configured by AD's policy
	uint8_t exp = 0; //expiration time
	//HC: pcb->propagated, pcb->age can be removed, as each customer
	//has a different set of selected paths

	for(cItr = childRange.first; cItr != childRange.second; cItr++){

		RouterElem cRouter = cItr->second;
#ifdef _HC_DEBUG_BS
		printf("HC_DEBUG: %llu:%llu progagating PCBs to %d\n", m_uAdAid, m_uAid, cItr->second.interface.id);
#endif
		std::map<uint16_t, SelectionPolicy>::iterator curPolicyIt = m_selPolicies.find(cRouter.interface.id);
		std::vector<pcb*> selPcbs = curPolicyIt->second.select(beacon_table);
		std::vector<pcb*>::iterator it;
		
		uint8_t nPropagated = 0;
		for(it = selPcbs.begin(); it != selPcbs.end(); ++it){
			
			// const pcb* curPcb = *it;
			// Tenma, make it changable
			pcb* curPcb = *it;				
			// if propagated, ignores.
			//HC ToDo: the same path should not be propagated to the same child again.
			//this should be dealt within path selection policy routine.

			//SJ TODO: Must check if the IFID is alive (by checking IFIDMAP)
			uint8_t msg[MAX_PKT_LEN];
			memset(msg, 0, MAX_PKT_LEN);
			memcpy(msg, curPcb->msg, curPcb->totalLength);

#ifdef ENABLE_AESNI
			keystruct ks;
			getOfgKey(SPH::getTimestamp(msg),ks);
#else
			aes_context actx;
			//SLT:TS
			//HC: why can't we get timestamp from pcb->timestamp directly?
			getOfgKey(SPH::getTimestamp(msg),actx);
#endif
			
			uint16_t sigLen = PriKey.len;
			
#ifdef ENABLE_AESNI
			SCIONBeaconLib::addLink(msg,curPcb->ingress,cRouter.interface.id, PCB_TYPE_TRANSIT, 
				m_uAdAid, m_uTdAid, &ks,0,exp,0, sigLen);
#else
			SCIONBeaconLib::addLink(msg,curPcb->ingress,cRouter.interface.id, PCB_TYPE_TRANSIT, 
				m_uAdAid, m_uTdAid, &actx,0,exp,0, sigLen);
#endif

			SPH::addDstAddr(msg, cRouter.addr);
			SCIONBeaconLib::setInterface(msg, cRouter.interface.id);

			//add all the peers
			for(pItr=peerRange.first;pItr!=peerRange.second;pItr++){
				RouterElem pRouter = pItr->second;
				std::map<uint16_t,uint16_t>::iterator iItr;
				
                if((iItr = ifid_map.find(pRouter.interface.id))==ifid_map.end()){
					#ifdef _SL_DEBUG_BS
                    printf("BS (%llu:%llu): Cannot Add Peering Link: IFID (%d) has not been registered\n", 
						m_uAdAid,m_uAid,pRouter.interface.id);
					#endif
                    scionPrinter->printLog(EH, "Cannot Add Peering Link: IFID (%d) has not been registered\n",
						pRouter.interface.id);
					//SL+: skip the interface that has not replied IFID_REQ
					continue;
				}
#ifdef ENABLE_AESNI
				SCIONBeaconLib::addPeer(msg,pRouter.interface.id, cRouter.interface.id, 
					iItr->second,PCB_TYPE_PEER, pRouter.interface.neighbor,1, &ks,0,0,0);
#else
                SCIONBeaconLib::addPeer(msg,pRouter.interface.id, cRouter.interface.id, 
					iItr->second,PCB_TYPE_PEER, pRouter.interface.neighbor,1, &actx,0,0,0);
#endif
				/*TODO*/     
				//SL: what is 1 in next to the last argument?
				//it's TDID;and needs to be defined in the topology file, and router class, and set here for inter-TD peering link
			}// end of for

			SCIONBeaconLib::signPacket(msg, PriKey.len, cRouter.interface.neighbor, &PriKey);
			uint16_t msgLength = SPH::getTotalLen(msg);
			//SLT:TS
			uint32_t ts = SPH::getTimestamp(msg);
			uint64_t dst = cRouter.interface.neighbor;
			scionPrinter->printLog(IH,BEACON,ts,m_uAdAid,dst,"%u,SENT\n",msgLength);
			sendPacket(msg, msgLength,PORT_TO_SWITCH, TO_ROUTER);
			
			nPropagated++;
			if(nPropagated >= m_iKval) break;
		}
	}
	
	return SCION_SUCCESS;
}


/*
    SCIONBeaconServer::verifyPcb
    - verifies the given PCB
    - iterates through PCB and verifies all the signatures 
      in the PCB. 
*/

uint8_t SCIONBeaconServer::verifyPcb(SPacket* pkt){
	
	uint8_t hdrLen = SPH::getHdrLen(pkt);
	pcbMarking* mrkPtr = (pcbMarking*)(pkt+hdrLen+OPAQUE_FIELD_SIZE*2);
	uint8_t* ptr = (uint8_t*)mrkPtr;
	uint8_t hops = SCIONBeaconLib::getNumHops(pkt); 

	uint16_t sigLen = mrkPtr->sigLen;
	uint16_t msgLen = mrkPtr->blkSize;
	uint8_t srcLen = SPH::getSrcLen(pkt);
	uint8_t dstLen = SPH::getDstLen(pkt);

	//signature
	uint8_t sig[sigLen];
	memcpy(sig, ptr+mrkPtr->blkSize, sigLen);
	
	//marking (Opaque Field + marking info)
	//SL: add next AID (8Bytes) 
	uint8_t msg[msgLen+SIZE_AID];
	memset(msg, 0, msgLen);
	memcpy(msg, ptr, msgLen);
	pcbMarking* nextPtr;
	
	//SL: chaining next AID (i.e., my AID)
	if(hops > 1) { 
		//if more than one signatures exist
		nextPtr = (pcbMarking*)(ptr + msgLen + sigLen);
		memcpy(&msg[msgLen], &(nextPtr->aid), SIZE_AID);
	} else {
		memcpy(&msg[msgLen], &m_uAdAid, SIZE_AID);
	}
	msgLen += SIZE_AID;

	// Part1: TDCore Signature Verification
	// Verify Using ROT structure
	// Get certificate from ROT
	x509_cert TDCert;
	int ret = 0;
	memset(&TDCert, 0, sizeof(x509_cert));
	ret = x509parse_crt(&TDCert, 
		(const unsigned char*)m_cROT.coreADs.find(mrkPtr->aid)->second.certificate,
		m_cROT.coreADs.find(mrkPtr->aid)->second.certLen);
	// read error?
	if( ret < 0 ) {
		#ifdef _SL_DEBUG_BS
		printf("BS (%llu:%llu): x509parse_crt parsing failed.\n", m_uAdAid, m_uAid);
		#endif
		x509_free( &TDCert );
		// TODO: Request a correct ROT with a pre-defined number of trials
		// Add it to Queue
		addUnverifiedPcb(pkt);
		return SCION_FAILURE;
	}

	if(SCIONCryptoLib::verifySig(msg, sig, msgLen, &(TDCert.rsa)) != scionCryptoSuccess) {
		#ifdef _SL_DEBUG_BS
		printf("BS (%llu:%llu): Signature verification (TDC) failure\n", m_uAdAid, m_uAid);
		#endif
		x509_free(&TDCert);
        return SCION_FAILURE;
	}else{
		x509_free( &TDCert );
	}
	
	// Non TDC signature verification
	uint8_t cert[MAX_FILE_LEN];
	x509_cert * adcert = NULL;
	std::map<uint64_t,x509_cert*>::iterator it;
	
	pcbMarking* prevMrkPtr = mrkPtr;
	ptr+=mrkPtr->blkSize+mrkPtr->sigLen;
	mrkPtr = (pcbMarking*)ptr;
	
	for(int i=1;i<hops;i++){
		
		////////////////////////////////////////////////////////////////////////
		//SL: The following routine is added to install filter against packet
		//duplication problem.
		if(mrkPtr->aid == m_uAdAid) {
			#ifdef _SL_DEBUG
			printf("Duplicate error possibly by a NIC driver\n");
			#endif
			return SCION_FAILURE;
		}
		////////////////////////////////////////////////////////////////////////

		msgLen = mrkPtr->blkSize;
		sigLen = prevMrkPtr->sigLen;

		int contentSize = msgLen + sigLen + SIZE_AID;

		uint8_t content[contentSize];
		memcpy(content, ptr, mrkPtr->blkSize);

		//get previous hop signature, Note: signatures are chained
		uint8_t* oldSigPtr = (uint8_t*)(prevMrkPtr)+prevMrkPtr->blkSize;
		memcpy(content+mrkPtr->blkSize, oldSigPtr, prevMrkPtr->sigLen);

		//now AID chaining
		if(i < hops -1) {
    		nextPtr = (pcbMarking*)(ptr + mrkPtr->blkSize + mrkPtr->sigLen);
			memcpy(content+msgLen+prevMrkPtr->sigLen, &nextPtr->aid, SIZE_AID);
		} else {
			memcpy(content+msgLen+prevMrkPtr->sigLen, &m_uAdAid, SIZE_AID);
		}
		//get current hop info
		uint8_t signature[mrkPtr->sigLen];
		memcpy(signature, ptr+mrkPtr->blkSize, mrkPtr->sigLen);
		
		//Load certificate for the current hop signature
		if((it = m_certMap.find(mrkPtr->aid)) == m_certMap.end()) {
			memset(cert, 0, MAX_FILE_LEN);
			getCertFile(cert, mrkPtr->aid); //filename retrieval
			
			FILE* cFile;
			if((cFile=fopen((const char*)cert,"r"))==NULL){
				#ifdef _SL_DEBUG_BS
				printf("BS (%llu:%llu): Cert of AD %llu does not exist.\n", m_uAdAid, m_uAid, mrkPtr->aid);
				#endif
				// Tenma: reverse the order here since unverified pcbs should be added
				// brefore we create cert request packets.
				// TODO: rewrite requestForCert for an elegant way
				// requestForCert increases pkt hop count and path information by addLink function call
				addUnverifiedPcb(pkt);
				requestForCert(pkt); 
				return SCION_FAILURE;
			}
			fclose(cFile);
					
			adcert = new x509_cert;
			memset(adcert,0,sizeof(x509_cert));

			int err = x509parse_crtfile(adcert, (const char*)cert);
			if(err){
				#ifdef _SL_DEBUG_BS
				printf("BS (%llu:%llu): fail to extract AD %llu's cert to verify signatures.\n", 
					m_uAdAid, m_uAid, mrkPtr->aid);
				#endif
				scionPrinter->printLog(EH, "BS(%llu:%llu): Error Loading Certificate for AD %llu\n", m_uAdAid, m_uAid, mrkPtr->aid);
				x509_free(adcert);
				delete adcert;
				return SCION_FAILURE;		
			}
		
			m_certMap.insert(std::pair<uint64_t,x509_cert*>(mrkPtr->aid, adcert));
		}
		else {
			adcert = it->second;
		}
		
		//verify this signature
		//SL: now subChain includes all certificates
		//need to verify certificate and cache the last one (do this once in a predefined period)
		//use x509parse_verify, Note: use "x509_cert->next"
			
		// Tenma: genSig only uses msg+aid content for signature.
		if(SCIONCryptoLib::verifySig(content,signature,msgLen+SIZE_AID,&(adcert->rsa))!=scionCryptoSuccess){
			#ifdef _SL_DEBUG_BS
			printf("BS (%llu:%llu): Signature verification failed for AD %llu\n", m_uAdAid, m_uAid, mrkPtr->aid);
			#endif
			x509_free(adcert);
			return SCION_FAILURE;
		}
		
		prevMrkPtr = mrkPtr;
		ptr += (mrkPtr->blkSize + mrkPtr->sigLen);
		mrkPtr = (pcbMarking*)ptr;
	}
	///////////////////////////////////////////////////////////
	//verification loop for the rest of the signatures ends here
	///////////////////////////////////////////////////////////
	//x509_free(&chain);
	return SCION_SUCCESS;
}



/*
    SCIONBeaconServer::getOfgKey
    - returns the OFG key for the given timestamp.
    - If the timestamp does not exsist, then creates,
      store in the key table and then returns the created
      key.
    - If the key table is full then remove the first entry (oldest)
      then add the new key
*/
#ifdef ENABLE_AESNI
bool SCIONBeaconServer::getOfgKey(uint32_t timestamp, keystruct &ks)
#else
bool SCIONBeaconServer::getOfgKey(uint32_t timestamp, aes_context &actx)
#endif
{

	if(timestamp > m_currOfgKey.time) {
#ifdef ENABLE_AESNI
		ks = m_currOfgKey.aesnikey;
#else
		actx = m_currOfgKey.actx;
#endif
	} else {
#ifdef ENABLE_AESNI
		ks = m_prevOfgKey.aesnikey;
#else
		actx = m_prevOfgKey.actx;
#endif
	}
    return SCION_SUCCESS;

#ifndef ENABLE_AESNI
	//SL: following part is left to compare performance later.
	//////////////////////////////////////////////////////////
    std::map<uint32_t, aes_context*>::iterator itr;

    //aes_context actx;
    memset(&actx, 0, sizeof(aes_context));

    //when the key table is full
    while(m_OfgAesCtx.size()>KEY_TABLE_SIZE){
		itr = m_OfgAesCtx.begin();
		delete itr->second;
        m_OfgAesCtx.erase(itr);
    }

    //if key for the timestamp is not found. 
    if((itr=m_OfgAesCtx.find(timestamp)) == m_OfgAesCtx.end()){

        //concat timestamp with the ofg master key.
        uint8_t k[SHA1_SIZE];
        memset(k, 0, SHA1_SIZE);
        memcpy(k, &timestamp, sizeof(uint32_t));
        memcpy(k+sizeof(uint32_t), m_uMkey, OFG_KEY_SIZE);

        //creates sha1 scionHash 
        uint8_t buf[SHA1_SIZE];
        memset(buf, 0 , SHA1_SIZE);
        sha1(k, TS_OFG_KEY_SIZE, buf);

        ofgKey newKey;
        memset(&newKey, 0, OFG_KEY_SIZE);
        memcpy(newKey.key, buf, OFG_KEY_SIZE);

		//SLT:TS
        key_table.insert(std::pair<uint32_t, ofgKey>(timestamp, newKey));

		//SL: OFG key aes context to avoid redundant key scheduling
		//store the aes_context in m_OfgAesCtx
		aes_context * pActx = new aes_context; 
		
		int err;
		if(err = aes_setkey_enc(pActx, newKey.key, OFG_KEY_SIZE_BITS)) {
			printf("Enc Key setup failure: %d\n",err*-1);
			return SCION_FAILURE;
		}

		m_OfgAesCtx.insert(std::pair<uint32_t, aes_context*>(timestamp, pActx));
		actx = *m_OfgAesCtx.find(timestamp)->second;

    }else{
		actx = *m_OfgAesCtx.find(timestamp)->second;
    }

    return SCION_SUCCESS;
#endif
}

int SCIONBeaconServer::sendHello()
{
    string msg;
    msg.append("0^");
    msg.append(MYAD);
    msg.append("^");
    msg.append(MYHID);
    msg.append("^");

    WritablePacket *p = Packet::make(DEFAULT_HD_ROOM, msg.c_str(), msg.size() + 1, DEFAULT_TL_ROOM);
    TransportHeaderEncap *thdr = TransportHeaderEncap::MakeDGRAMHeader(0); // length
	WritablePacket *q = thdr->encap(p);

	XIAPath src, dst;
	src.parse(m_sdag);
	dst.parse(m_ddag);

	XIAHeaderEncap encap;
	encap.set_src_path(src);
	encap.set_dst_path(dst);
    encap.set_plen(msg.size() + 1 + thdr->hlen());

	output(0).push(encap.encap(q, false));
    return 0;
}

void 
SCIONBeaconServer::sendPacket(SPacket* data, uint16_t data_length, int port, int fwd_type){
	//SLA:
	//IPV4 Handling
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


/*
    SCIONBeaconServer::getCertFile
    - get the file path for the certificat file for the given target AD.
    - Subject to be changed when certificate server is ready. 
*/
void SCIONBeaconServer::getCertFile(uint8_t* fn, uint64_t target){
    sprintf((char*)fn,"./TD1/Non-TDC/AD%llu/beaconserver/certificates/td%llu-ad%llu-0.crt",m_uAdAid,m_uTdAid,target); 
}



void SCIONBeaconServer::recheckPcb(){
	std::multimap<uint32_t, pcb>::iterator itr;
	for(itr=unverified_pcbs.begin();itr!=unverified_pcbs.end();itr++){
		if(verifyPcb(itr->second.msg)==SCION_SUCCESS){
			addPcb(itr->second.msg);
			free(itr->second.msg);
			unverified_pcbs.erase(itr);
		}
	}
	//unverified_pcbs.clear();   
}

/*
	Request all missing certificates for PCB verification
	to Certificate Server
*/
void SCIONBeaconServer::requestForCert(SPacket* pkt){

	uint8_t hdrLen = SPH::getHdrLen(pkt);
	pcbMarking* mrkPtr = (pcbMarking*)(pkt+hdrLen+OPAQUE_FIELD_SIZE*2);
	uint8_t* ptr = (uint8_t*)mrkPtr;
	uint8_t hops = SCIONBeaconLib::getNumHops(pkt);

	uint8_t srcLen = SPH::getSrcLen(pkt);
	uint8_t dstLen = SPH::getDstLen(pkt);
    
	certReq newReq = certReq();
	newReq.numTargets=0;

	// construct the list of all missing certificates
	for(int i=0;i<hops;i++){
		uint8_t fileName[MAX_FILE_LEN];
		// todo: more elegant solution to skip TDC cert request
		getCertFile(fileName, mrkPtr->aid);
		FILE* certFile=fopen((const char*)fileName, "r");
		// we should skip TDC cert request, not just use condition to check
		//SL: check the current certRequest to avoid pending request
		//However, retry should also be made if timeout occurs...
		//add timeout to pending request.. and retry...
		if(certFile==NULL && certRequest.find(mrkPtr->aid)==certRequest.end() && (mrkPtr->aid!=1)){
			certRequest[mrkPtr->aid]=1;
			newReq.targets[newReq.numTargets]=mrkPtr->aid;
			newReq.numTargets++;
		}else if(certFile!=NULL){
			fclose(certFile);
		}
		//SL: signature length might be available in the signature; need to check
		//move on to the next hop AD
		ptr=(uint8_t*)(ptr+mrkPtr->blkSize+mrkPtr->sigLen);
		mrkPtr = (pcbMarking*)(ptr);
	}
    
	if(newReq.numTargets==0){
		return;
		// nothing to request
	}
	
	//Add its own opaque field
	//this is necessary to send a request to upstream, since the egress router should check OF
#ifdef ENABLE_AESNI
	keystruct ks;
	getOfgKey(SPH::getTimestamp(pkt),ks);
#else
	aes_context actx;
	getOfgKey(SPH::getTimestamp(pkt),actx);
#endif
	
	uint16_t tmpPacketLength = SPH::getTotalLen(pkt);
	tmpPacketLength += PCB_MARKING_SIZE; //reserve space for an additional marking
	uint8_t tmpPacket[tmpPacketLength];  //for copying the original packet to the increased buffer
	memcpy(tmpPacket,pkt,tmpPacketLength-PCB_MARKING_SIZE);

	uint16_t ingIf = SCIONBeaconLib::getInterface(pkt);
	
#ifdef ENABLE_AESNI
	SCIONBeaconLib::addLink(tmpPacket, ingIf, NON_PCB, 1, m_uAdAid, m_uTdAid, &ks, 0, 0, 0, (uint16_t)PriKey.len);
#else
	SCIONBeaconLib::addLink(tmpPacket, ingIf, NON_PCB, 1, m_uAdAid, m_uTdAid, &actx, 0, 0, 0, (uint16_t)PriKey.len);
#endif
	hops = hops+1;
	
	//additional OF for itself since PCB has not been marked yet
	uint8_t path[OPAQUE_FIELD_SIZE*hops];
	buildPath(tmpPacket, path);

	HostAddr certAddr = m_servers.find(CertificateServer)->second.addr;
	HostAddr srcAddr = HostAddr(HOST_ADDR_SCION,m_uAid);

	hdrLen = COMMON_HEADER_SIZE+srcAddr.getLength()+certAddr.getLength()+hops*OPAQUE_FIELD_SIZE;
	// CERT_REQ_SIZE = 88, should be dynamic allocate instead of a fixed value
	uint8_t packetLength = hdrLen+CERT_REQ_SIZE;
    
	//build certificate request packet
	uint8_t newPacket[packetLength];
	
	scionHeader hdr;
	//set common header
	hdr.cmn.type = CERT_REQ_LOCAL;
	hdr.cmn.hdrLen = hdrLen;
	hdr.cmn.totalLen = packetLength;
	
	//set src/dst addresses
	hdr.src = srcAddr;
	hdr.dst = certAddr;

	//set opaque fields
	hdr.n_of = hops;
	hdr.p_of = path;
	
	SPH::setHeader(newPacket,hdr);
	
	// copy CertReq Structure
	memcpy(newPacket+hdrLen, &newReq, CERT_REQ_SIZE);
	printf("BS (%llu:%llu): Request Cert to CS (%llu)\n", m_uAdAid, m_uAid,certAddr.numAddr());
	sendPacket(newPacket, packetLength, PORT_TO_SWITCH, TO_SERVER);
}

void 
SCIONBeaconServer::clearCertificateMap() {
	std::map<uint64_t, x509_cert*>::iterator it;
	x509_cert * pChain;
	for(it=m_certMap.begin(); it!=m_certMap.end(); it++){
	    pChain = it->second;
		x509_free(pChain);
		delete pChain;
	}
	m_certMap.clear();
}

void 
SCIONBeaconServer::clearAesKeyMap() {
	std::map<uint32_t, aes_context*>::iterator it;
	aes_context * actx;
	for(it=m_OfgAesCtx.begin(); it!=m_OfgAesCtx.end(); it++){
	    actx = it->second;
		delete actx;
	}
	m_OfgAesCtx.clear();
	//now exiting the process
	fprintf(stderr,"AD%llu: Process terminates correctly.\n", m_uAdAid);
}


// HC: 
// TODO: load policies from configuration
// Currently assign a simple policy for every child for testing purpose 
void SCIONBeaconServer::initSelectionPolicy(){
    std::multimap<int, RouterElem>::iterator cItr;

    //find all the child routers
    std::pair<std::multimap<int, RouterElem>::iterator,
        std::multimap<int,RouterElem>::iterator> childRange;
    childRange = m_routers.equal_range(Child);
#ifdef _HC_DEBUG_BS
    printf("HC_DEBUG: numChildren=%d k=%d beacon_table_size=%d\n", m_routers.count(Child), m_iKval, m_iBeaconTableSize);
#endif    
    
    for(cItr = childRange.first; cItr != childRange.second; ++cItr){
        RouterElem router = cItr->second;
        SelectionPolicy tmpPl(m_iKval);
        //randomly selected parameters for testing purpose
        tmpPl.setExclusion(2000, 8, 100, std::tr1::unordered_set<uint64_t>());
        tmpPl.setWeight(0.4, 0.4, 0.2);
        tmpPl.setSelectionProbability(0.9);
        m_selPolicies.insert (std::pair<uint16_t, SelectionPolicy>(cItr->second.interface.id, tmpPl) );
    }
}

bool SelectionPolicy::setSelectionProbability(double p){
    if(p < 0.0 || p > 1.0) return false;
    m_dSelProb = p;
    return true;
}

//HC: SelectionPolicy class
//return false if the values are not set
bool SelectionPolicy::setExclusion(time_t age, uint8_t len, time_t elapsed,
                                   const std::tr1::unordered_set<uint64_t>& ADs){
    if(age <= 0 || len <= 0 || elapsed < 0) return false;
    m_tMaxAge = age;
    m_iMaxLen = len;
    m_tMinElapsedTime = elapsed;
    m_excludedADs = ADs;
    return true;
}

//return false if the values are not set
bool SelectionPolicy::setWeight(double age, double len, double elapsed){
    double sum = age + len + elapsed;
    if(age < 0.0 || len < 0.0 || elapsed < 0.0) return false;
    //normalize them in case sum =\=1
    m_dAgeWT = age/sum;
    m_dLenWT = len/sum;
    m_dElapsedTimeWT = elapsed/sum;
    
    return true;
}

//return true if the candidate pcb is excluded by the policy
bool SelectionPolicy::isExcluded(time_t curTime, const pcb& candidate){

    //rule 1: check if the age exceeds m_tMaxAge
    if(curTime - m_tMaxAge > candidate.timestamp) return true;

    //rule 2: check if the length exceeds m_iMaxLen
    if(candidate.hops > m_iMaxLen) return true;

    //rule 3: check if we have selected this path recently
    scionHash nhash = createHash(candidate.msg);
    std::tr1::unordered_map<scionHash, time_t>::iterator it;
    if( (it = digest.find(nhash)) != digest.end()) {
        if((curTime - it->second) < m_tMinElapsedTime)
            return true;
    }
    //rule 4: check if containing any unwanted AD
    //HC: is there a better way to retrieve the AIDs?
    uint8_t hdrLen = SPH::getHdrLen(candidate.msg); 
    uint8_t* ptr = candidate.msg+hdrLen+OPAQUE_FIELD_SIZE*2;
    pcbMarking* mrkPtr = (pcbMarking*)ptr;
    for(size_t i = 0; i < candidate.hops; ++i){
        std::tr1::unordered_set<uint64_t>::iterator it = m_excludedADs.find(mrkPtr->aid);
        if(it != m_excludedADs.end()) return true;
        ptr+=mrkPtr->sigLen + mrkPtr->blkSize;
        mrkPtr = (pcbMarking*)ptr;
    }
    return false;
}

// Return the priority (a double type) of a pcb according to the
// policy.  A smaller priority value means the path is more preferred.
double SelectionPolicy::computePriority(time_t curTime, const pcb& candidate){
    double normAge = (double)((uint32_t) curTime - candidate.timestamp)/m_tMaxAge;
    double normLen = (double) candidate.hops/m_iMaxLen;
    scionHash nhash = createHash(candidate.msg);
    std::tr1::unordered_map<scionHash, time_t>::iterator it;
    it = digest.find(nhash);
    double normFresh = (it == digest.end()) ? 0 : (double)m_tMinElapsedTime/(curTime - it->second);
    double priValue = normAge * m_dAgeWT + normLen * m_dLenWT + normFresh * m_dElapsedTimeWT;
#ifdef _HC_DEBUG_BS
    printf("HC_DEBUG: age=%d hops=%d normElapsed=%f priorityValue=%f\n", curTime - candidate.timestamp, candidate.hops, normFresh, priValue);
#endif    
    //TODO: consider other aspects such as disjointness
    return priValue;
}

void SelectionPolicy::updateDigest(const pcb* p){
#ifdef _HC_DEBUG_BS
    printPcb(p); //test
#endif    
    //TODO: keep a summary of recently selected paths, which can be
    //used to compute inter-path characteristics such as disjointness
    time_t curTime;
	time(&curTime);
	//SL: commented out since it's not used
    //digest.insert(std::pair<scionHash, time_t>(createHash(p->msg), curTime));
}

// Return selected paths, maximum size is m_iNumSelectedPath
std::vector<pcb*> SelectionPolicy::select(const std::multimap<uint32_t, pcb> &availablePaths){

    time_t curTime;
	time(&curTime);
#ifdef _HC_DEBUG_BS
    printf("HC_DEBUG: curTime %d\n", curTime);
#endif    
    std::vector<pcb*> selectedPcbs;
    std::priority_queue<PathPriority, std::deque<PathPriority> > pcbPriQueue;
    std::multimap<uint32_t, pcb>::const_reverse_iterator itr;
    std::tr1::unordered_set<scionHash, mySetHash, mySetEqual> duplicateFilter;

    int i = 0;
    for(itr = availablePaths.rbegin(); itr != availablePaths.rend(); ++itr){
#ifdef _HC_DEBUG_BS
        printf("HC_DEBUG: beacon_table line=%d, curTime=%d, ts=%d\n", i++, curTime, itr->first);
        printPcb(&itr->second); //test
#endif
        // AvailablePaths are checked in descending order (of
        // timestamp), so we can stop exploring when age > m_tMaxAge
        if((curTime - m_tMaxAge) > itr->first) break;
        // randomized selection process
        if((double)rand()/RAND_MAX > m_dSelProb) continue;
        // Duplicate detection
        scionHash nHash = createHash(itr->second.msg);
        if(duplicateFilter.find(nHash) != duplicateFilter.end()) continue;
        duplicateFilter.insert(nHash);

        //check 1: Exclusion policy -- skip excluded paths
        if(isExcluded(curTime, itr->second)) continue;
        //check 2: Prioritization policy -- keep at most
        //m_iNumSelectedPath most preferred paths
        double pri = computePriority(curTime, itr->second);
        pcbPriQueue.push(PathPriority(pri, &itr->second));
        if(pcbPriQueue.size() > m_iNumSelectedPath){
            //pop the one with the highest value (least preferred)
            pcbPriQueue.pop(); 
        }
    }
    //update digest and prepare to return selected pcbs
    while(!pcbPriQueue.empty()){
        selectedPcbs.push_back((pcb*)pcbPriQueue.top().cpcb);
        updateDigest((const pcb*)(pcbPriQueue.top().cpcb)); 
        pcbPriQueue.pop();
    }
    return selectedPcbs;
}

// HC: I copied this from scionpathserver.cc, except that timestamp is also included Here
// but we should define this in SCION library in the future.
scionHash SelectionPolicy::createHash(SPacket* pkt){
    uint8_t hops = SCIONBeaconLib::getNumHops(pkt);
	uint32_t ts = SPH::getTimestamp(pkt);
    uint16_t outputSize = hops*(sizeof(uint16_t)*2+sizeof(uint64_t)) + sizeof(uint32_t);
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
    *(uint32_t*)(buf+unit*hops) = ts; //include timestamp as well
    uint8_t sha1Hash[SHA1_SIZE];
    memset(sha1Hash, 0,SHA1_SIZE);
    sha1((uint8_t*)buf, outputSize, sha1Hash);

    scionHash newHash = scionHash();
    memset(newHash.hashVal,0, SHA1_SIZE);
    memcpy(newHash.hashVal, sha1Hash, SHA1_SIZE);
    return newHash;
}

void SelectionPolicy::printPcb(const pcb* p){
    uint8_t hdrLen = SPH::getHdrLen(p->msg); 
    uint8_t* ptr = p->msg+hdrLen+OPAQUE_FIELD_SIZE*2;
    pcbMarking* mrkPtr = (pcbMarking*)ptr;
    printf("HC_DEBUG: timestamp: %u hops: %u | ", p->timestamp, p->hops);
    for(size_t i = 0; i < p->hops; ++i){
        printf("%llu(%d:%d) | ", mrkPtr->aid,  mrkPtr->ingressIf, mrkPtr->egressIf);
        ptr+=mrkPtr->sigLen + mrkPtr->blkSize;
        mrkPtr = (pcbMarking*)ptr;
    }
    printf("\n");
    
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONBeaconServer)

