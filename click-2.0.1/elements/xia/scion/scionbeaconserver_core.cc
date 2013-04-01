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

#include "scionbeaconserver_core.hh"
#include "rot_parser.hh"

CLICK_DECLS

int SCIONBeaconServerCore::configure(Vector<String> &conf, ErrorHandler *errh) {
	if(cp_va_kparse(conf, this, errh,
		"AID", cpkM, cpUnsigned64,&m_uAid, 
		"CONFIG_FILE", cpkM, cpString, &m_sConfigFile, 
		"TOPOLOGY_FILE",cpkM, cpString, &m_sTopologyFile,
		"ROT", cpkM, cpString, &m_sROTFile,	// Tenma, tempral exist but should merge into config class
		cpEnd) <0) {
		printf("ERR: click configuration fail at SCIONBeaconServerCore.\n");
		printf("ERR: Fault error, exit SCION Network.\n");
		exit(-1);
    }
    return 0;
}

int SCIONBeaconServerCore::initialize(ErrorHandler* errh) {
	
	// task1: Config object get ROT, certificate file, private key path info
	Config config; 
	config.parseConfigFile((char*)m_sConfigFile.c_str());
	config.getPrivateKeyFilename((char*)m_csPrvKeyFile);
	config.getCertFilename((char*)m_csCertFile);
	config.getOfgmKey((char*)m_uMkey);
	config.getPCBLogFilename(m_csLogFile);
	
	// get AID, ADAID, TDID
	m_uAdAid = config.getAdAid();
	m_uTdAid = config.getTdAid();
	m_iLogLevel = config.getLogLevel();
	m_iPCBGenPeriod = config.getPCBGenPeriod();

	// setup looger, scionPrinter
	scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile); 
	scionPrinter->printLog(IL, "TDC BS (%llu) INIT.\n", m_uAid);
	scionPrinter->printLog(IL, "ADAID = %llu, TDID = %llu.\n", m_uAdAid, m_uTdAid);

	// task 2: parse ROT (root of trust) file
	m_bROTInitiated = parseROT();
	scionPrinter->printLog(IL, "Parse/Verify ROT Done.\n");

	// task 3: parse topology file
	parseTopology();
	scionPrinter->printLog(IL, "Parse Topology Done.\n");
	
	// task 4: read key pair if they already exist
	loadPrivateKey();
	scionPrinter->printLog(IL, "Load Private Key Done.\n");
	// for OFG fields
	if(initOfgKey()==SCION_FAILURE){
		scionPrinter->printLog(EH, "Init OFG key failure.\n");
		printf("ERR: InitOFGKey fail at SCIONBeaconServerCore.\n");
		printf("ERR: Fatal error, exit SCION Network.\n");
		exit(-1);
	}
	if(updateOfgKey()==SCION_FAILURE){
		scionPrinter->printLog(IL, "Update OFG key failure.\n");
        printf("ERR: updateOFGKey fail at SCIONBeaconServerCore.\n");
        printf("ERR: Fatal error, exit SCION Network.\n");
		exit(-1);
	}
	scionPrinter->printLog(IL, "Load OFG key Done.\n");
	
	// start scheduler to received packets
	ScheduleInfo::initialize_task(this, &_task, errh);
	// Trigger Timer
	_timer.initialize(this);
	//SL:
	//start after other elements are initialized
	//this part should be considered more carefully later.
	//e.g., how to handle PCBs propagated before others set up (e.g., ifid config)
	_timer.schedule_after_sec(5);
	//_timer.schedule_now();
	
	return 0;
}

/*SL: Initialize OFG key */
bool SCIONBeaconServerCore::initOfgKey() {
	
	time_t currTime;
	time(&currTime);

	//SL: to deal with the case that TDC BS initiate PCB before this BS starts
	currTime -= 300; //back 5minutes

	m_currOfgKey.time = currTime;
	memcpy(m_currOfgKey.key, &m_uMkey, OFG_KEY_SIZE);

	int err;
	if(err = aes_setkey_enc(&m_currOfgKey.actx, m_currOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}
	return SCION_SUCCESS;
}

/*SL: Update OFG key when a new key is available from CS*/
bool SCIONBeaconServerCore::updateOfgKey() {
	//This function should be updated...
	m_prevOfgKey.time = m_currOfgKey.time;
	memcpy(m_prevOfgKey.key, m_currOfgKey.key, OFG_KEY_SIZE);

	int err;
	if(err = aes_setkey_enc(&m_prevOfgKey.actx, m_prevOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Prev. Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}
	return SCION_SUCCESS;
}


void SCIONBeaconServerCore::parseTopology(){
    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
    parser.parseRouters(m_routers);
}

void SCIONBeaconServerCore::loadPrivateKey() {
    
    if(!_CryptoIsReady) {
	rsa_init(&PriKey, RSA_PKCS_V15, 0);
    	int err = x509parse_keyfile(&PriKey, m_csPrvKeyFile, NULL);	
        if(err) {
          rsa_free(&PriKey);
          cerr << "ERR: Private key file loading failure, path = " << m_csPrvKeyFile << endl;
	  cerr << "Exit SCION Network." << endl;
	  exit(0);
	}
	if(rsa_check_privkey(&PriKey)!=0) {
	  rsa_free(&PriKey);
          cerr << "ERR: Private key is not well-formatted, path = " << m_csPrvKeyFile << endl;
	  cerr << "Exit SCION Network." << endl;
	  exit(0);
	}	
	_CryptoIsReady = true;
    }  
}



bool SCIONBeaconServerCore::run_task(Task *task) {

	Packet* inPacket;
	//handle incoming packet from queue 
	while((inPacket = input(0).pull())){
		//copy packet data and kills click packet
		uint16_t packetLength = SCIONPacketHeader::getTotalLen((uint8_t*)inPacket->data());
		uint8_t srcLen = SCIONPacketHeader::getSrcLen((uint8_t*)inPacket->data());
		uint8_t dstLen = SCIONPacketHeader::getDstLen((uint8_t*)inPacket->data());
		uint8_t packet[packetLength];
		memset(packet, 0, packetLength);
		memcpy(packet, (uint8_t*)inPacket->data(), packetLength);
		inPacket->kill();
		uint16_t type = SCIONPacketHeader::getType(packet);
		// uint32_t ts = SCIONPacketHeader::getTimestamp(packet);
		uint32_t ts = 0;

		switch(type) {
		case ROT_REP_LOCAL:
		{
			#ifdef _SL_DEBUG_BS
			printf("TDC BS (%llu:%llu): Received ROT Reply from TDC CS.\n", m_uAdAid, m_uAid); 
			#endif

			// open a file with 0 size
			int RotL = packetLength-(COMMON_HEADER_SIZE+srcLen+dstLen);
			
			#ifdef _SL_DEBUG_BS
			printf("TDC BS (%llu:%llu): Received ROT file size = %d\n", m_uAdAid, m_uAid, RotL);
			#endif
			
			// Write to file defined in Config
			FILE * rotFile = fopen(m_sROTFile.c_str(), "w+");
			fwrite(packet+COMMON_HEADER_SIZE+srcLen+dstLen, 1, RotL, rotFile);
			fclose(rotFile);
			scionPrinter->printLog(IL, "TDC BS (%llu:%llu) stored receivd ROT.\n", m_uAdAid, m_uAid);
			// try to verify local ROT again!
			m_bROTInitiated = parseROT();
		}	break;
		
		case AID_REQ:
			#ifdef _SL_DEBUG_BS
			printf("TDC BS (%llu:%llu): Received AID_REQ from switch.\n", m_uAdAid, m_uAid); 
			#endif

			// AID_REQ, reply AID_REP packet to switch	
			scionPrinter->printLog(IH,type,ts,1,1,"%u,RECEIVED\n",packetLength);
			SCIONPacketHeader::setType(packet, AID_REP);
			SCIONPacketHeader::setSrcAddr(packet, HostAddr(HOST_ADDR_SCION,m_uAid));
			sendPacket(packet, packetLength, PORT_TO_SWITCH);
			_AIDIsRegister = true;	
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
       	default:
			/* Unsupported packets */
			printf("TDC BS (%llu:%llu): Unsupported type (%d) packet.\n",  m_uAdAid, m_uAid,type);
			break;
		}
	}//end of while
	_task.fast_reschedule();
}


bool SCIONBeaconServerCore::generateNewPCB() {

	//Create PCB for propagation

	// get Signature length
	uint16_t sigLen = PriKey.len;

	//get current time for timestamp
	timeval tv; //SL: can be changed to time_t instead... if lower precision (i.e., second) is sufficient 
	gettimeofday(&tv, NULL); //use time for lower precision

	//create packet data
	uint8_t hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
	uint16_t totalLen =hdrLen+OPAQUE_FIELD_SIZE*2+PCB_MARKING_SIZE+sigLen;
	uint8_t buf[totalLen];
	memset(buf, 0, totalLen); //empty scion header
	SCIONBeaconLib::initBeaconInfo(buf,tv.tv_sec,0,m_cROT.version); //SL: {packet,timestamp,tdid,ROT version} 

	scionHeader hdr;
	HostAddr srcAddr = HostAddr(HOST_ADDR_SCION,m_uAdAid); //put ADAID as a source address
	HostAddr dstAddr = HostAddr(HOST_ADDR_SCION,(uint64_t)0); //destination address will be set to egress router's address

	hdr.cmn.type = BEACON;
	hdr.cmn.hdrLen = COMMON_HEADER_SIZE+srcAddr.getLength()+dstAddr.getLength();
	hdr.cmn.totalLen = hdrLen+OPAQUE_FIELD_SIZE*2; //two opaque fields are used by TDC AD

	hdr.src = srcAddr;
	hdr.dst = dstAddr;

	SCIONPacketHeader::setHeader(buf,hdr);

	// TimeStamp OFG field
	specialOpaqueField sOF = {SPECIAL_OF,tv.tv_sec,m_uTdAid,0}; //SLT: {type, timestamp, tdid, hops}
	SCIONPacketHeader::setTimestampOF(buf, sOF);
	#ifdef _SL_DEBUG_BS
	printf("TDC BS (%llu:%llu): timestamp = %d, getTimestamp = %d, total Len = %d\n", m_uAdAid, m_uAid, tv.tv_sec, 
	SCIONPacketHeader::getTimestamp(buf), SCIONPacketHeader::getTotalLen(buf));
	#endif

	//iterators
	std::multimap<int, RouterElem>::iterator cItr; //child routers iterators

	//find peer routers
	std::pair<std::multimap<int, RouterElem>::iterator,
	std::multimap<int,RouterElem>::iterator> peerRange;
	peerRange = m_routers.equal_range(Peer);

	//find child routers
	std::pair<std::multimap<int, RouterElem>::iterator,
	std::multimap<int,RouterElem>::iterator> childRange;
	childRange = m_routers.equal_range(Child);

	//get the OFG key that corresponds to the current timestamp and create aes context (i.e., key schedule)
	aes_context actx; 
	if(getOfgKey(tv.tv_sec, actx)) {
		printf("TDC BS (%llu:%llu): fatal error, fail to  get ofg key.\n", m_uAdAid, m_uAid);
		//OFG key retrieval failure.
		//print log and continue.
		return SCION_FAILURE; //SL: need to define more specific error type
	}

	//iterate for all child routers and their IFIDs    
	for(cItr=childRange.first;cItr!=childRange.second;cItr++){
		uint8_t msg[MAX_PKT_LEN];
		memset(msg, 0, MAX_PKT_LEN);
		memcpy(msg, buf, SCIONPacketHeader::getTotalLen(buf));
		RouterElem router = cItr->second;
		//PCB_TYPE_CORE indicates the initial marking by Core BS
		//SL: Expiration time needs to be configured
		uint8_t exp; //expiration time
		SCIONBeaconLib::addLink(msg,0,router.interfaceID, PCB_TYPE_CORE, m_uAdAid, m_uTdAid, &actx, 0, exp, 0, sigLen);
		//for SCION switch to forward packet to the right egress router/interface
		//TODO:Addr
		SCIONPacketHeader::setDstAddr(msg, router.addr);
		SCIONBeaconLib::setInterface(msg, router.interfaceID);

		//sign the above marking
		SCIONBeaconLib::signPacket(msg, sigLen, router.neighbor, &PriKey);
		uint16_t msgLength = SCIONPacketHeader::getTotalLen(msg);
		//SL: why is this necessary? (setting ADAID to the source address)
		//TODO:Addr
		//SCIONPacketHeader::setSrcAid(msg, m_uAdAid);
		scionPrinter->printLog(IH,BEACON,tv.tv_sec,1,router.neighbor,"%u,SENT\n",msgLength);
		sendPacket(msg, msgLength,PORT_TO_SWITCH);
	}
}

/*
    SCIONBeaconServerCore::run_timer
	Periodically generate PCB
*/
void SCIONBeaconServerCore::run_timer(Timer *){

	if(m_bROTInitiated){
		// ROT file is ready
		if(_CryptoIsReady&&_AIDIsRegister) {
			// private key is ready
			#ifdef _SL_DEBUG_BS
			printf("TDC BS (%llu:%llu): Generate a New PCB.\n",m_uAdAid, m_uAid);
			#endif

			generateNewPCB();
    	}
		_timer.schedule_after_sec(m_iPCBGenPeriod);
	}else{
		#ifdef _SL_DEBUG_BS
		printf("TDC BS (%llu:%llu): ROT is missing or wrong formatted.\n", m_uAdAid, m_uAid);
		#endif

		// Send ROT_REQ_LOCAL while AID Registration Done
		if(_AIDIsRegister) {
			#ifdef _SL_DEBUG_BS
			printf("TDC BS (%llu:%llu): Send ROT Request to SCIONCertServerCore.\n", m_uAdAid, m_uAid);
			#endif

			HostAddr srcAddr = HostAddr(HOST_ADDR_SCION,m_uAid);
			HostAddr dstAddr = m_servers.find(CertificateServer)->second.addr;

			uint8_t hdrLen = COMMON_HEADER_SIZE+srcAddr.getLength()+dstAddr.getLength();
			uint16_t totalLen = hdrLen + ROT_VERSION_SIZE;
			uint8_t packet[totalLen];

			scionHeader hdr;

			hdr.cmn.type = ROT_REQ_LOCAL;
			hdr.cmn.hdrLen = hdrLen;
			hdr.cmn.totalLen = totalLen;

			hdr.src = srcAddr;
			hdr.dst = dstAddr;

			SCIONPacketHeader::setHeader(packet, hdr);

			// version number, try to get 0
			//SL: version number should be read from a configuration file
			*(uint32_t*)(packet+hdrLen) = 0;
			sendPacket(packet, totalLen, PORT_TO_SWITCH);
		}
		_timer.schedule_after_sec(1); 		// default speed
	}
}

/*
    SCIONBeaconServerCore::getOfgKey
    - finds OFG key for this timestamp.
    - If it does not exsists then creates new key and stores. 
*/

/*
bool SCIONBeaconServerCore::getOfgKey(const uint32_t &timestamp, aes_context& actx){

    std::map<uint32_t, ofgKey>::iterator itr;
    
    //aes_context actx;
    
	//SL: should actx be initialized?
	//memset(&actx, 0, sizeof(aes_context));
    
    //if OFG key is not found for the given timestamp
	//generate a new key using its master OFG key
	//SL: TDC BS always cannot find an existing key; i.e., the following is always true...
	//Alternatively, we can use a single OFG key and produce different MACs by incorporating timestamp in generating MAC
    if((itr=key_table.find(timestamp)) == key_table.end()){

        //concat timestamp with ofg master key
        uint8_t k[SHA1_SIZE];
        memset(k, 0, SHA1_SIZE);
        memcpy(k, &timestamp, sizeof(uint32_t));
        memcpy(k+sizeof(uint32_t), m_uMkey, OFG_KEY_SIZE);

        //create sha1 hash
        uint8_t buf[SHA1_SIZE];
        memset(buf, 0 , SHA1_SIZE);
        sha1(k, TS_OFG_KEY_SIZE, buf);

        //use 16 bytes out of 20 bytes
        ofgKey newKey;
        //SL:unnecessary
		//memset(&newKey, 0, OFG_KEY_SIZE);
        memcpy(newKey.key, buf, OFG_KEY_SIZE);
        key_table.insert(std::pair<uint32_t, ofgKey>(timestamp, newKey));
        
        aes_setkey_enc(&actx, newKey.key, OFG_KEY_SIZE_BITS);
    }else{
        aes_setkey_enc(&actx, itr->second.key, OFG_KEY_SIZE_BITS);
    }
    return SCION_SUCCESS; //return key schedule
}
*/

bool  SCIONBeaconServerCore::getOfgKey(const uint32_t &timestamp, aes_context &actx){

	if(timestamp > m_currOfgKey.time) {
		actx = m_currOfgKey.actx;
	} else {
		actx = m_prevOfgKey.actx;
	}
	return SCION_SUCCESS;

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

    //if key for the timestmpa is not found. 
    //if((itr=key_table.find(timestamp)) == key_table.end()){
    if((itr=m_OfgAesCtx.find(timestamp)) == m_OfgAesCtx.end()){

        //concat timestamp with the ofg master key.
        uint8_t k[SHA1_SIZE];
        memset(k, 0, SHA1_SIZE);
        memcpy(k, &timestamp, sizeof(uint32_t));
        memcpy(k+sizeof(uint32_t), m_uMkey, OFG_KEY_SIZE);

        //creates sha1 hash 
        uint8_t buf[SHA1_SIZE];
        memset(buf, 0 , SHA1_SIZE);
        sha1(k, TS_OFG_KEY_SIZE, buf);

        ofgKey newKey;
        memset(&newKey, 0, OFG_KEY_SIZE);
        memcpy(newKey.key, buf, OFG_KEY_SIZE);
        //key_table.insert(std::pair<uint32_t, ofgKey>(timestamp, newKey));

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
}

/*
    SCIONBeaconServerCore::sendPacket
    - Creates click packet and sends packet to the given port
*/
void SCIONBeaconServerCore::sendPacket(uint8_t* data, uint16_t dataLength, int port){
	#ifdef _SL_DEBUG_BS
	printf("BS (%llu:%llu): sending a packet (%dB) to port (%d)\n", m_uAdAid, m_uAid, dataLength, port);
	#endif
	//SLA:
	//IPv4 handling here
	//if addrtype == IPv4, encap with IP header
	//otherwise
	WritablePacket* outPacket = Packet::make(DEFAULT_HD_ROOM, data, dataLength, DEFAULT_TL_ROOM);
	output(port).push(outPacket);
}

bool SCIONBeaconServerCore::parseROT(){
	ROTParser parser;
	if(parser.loadROTFile(m_sROTFile.c_str())!=ROTParseNoError){
		scionPrinter->printLog(IL, "ERR: ROT File missing at SCIONBeaconServerCore.\n");
		return false;
	}else{
		// ROT found in local folder
		if(parser.parse(m_cROT)!=ROTParseNoError){
			scionPrinter->printLog(IL, "ERR: ROT File parsing error at SCIONBeaconServerCore.\n");
			return false;
		}

		if(parser.verifyROT(m_cROT)!=ROTParseNoError){
			scionPrinter->printLog(IL, "ERR: ROT File parsing error at SCIONBeaconServerCore.\n");
			return false;
		}
	}
	return true;
}

/*
	SLN:
	update ifid_map
*/
void
SCIONBeaconServerCore::updateIfidMap(uint8_t * packet) {
	uint32_t ts = 0;
	uint64_t src = 0;
				
	uint8_t hdrLen = SCIONPacketHeader::getHdrLen(packet);
	uint16_t nifid = *(uint16_t*)(packet+hdrLen);  
	uint16_t ifid = *(uint16_t*)(packet+hdrLen+IFID_SIZE);
	ifid_map.insert(pair<uint16_t, uint16_t>(ifid, nifid));

	#ifdef _SL_DEBUG_BS
	printf("BS (%llu:%llu): IFID received (neighbor:%d - self:%d)\n", m_uAid,m_uAdAid, nifid, ifid);
	#endif

	scionPrinter->printLog(IH, "BS (%llu:%llu): IFID received (neighbor:%d - self:%d)\n", m_uAid,m_uAdAid, nifid, ifid);
	//SL:
	//print information needs to be revised everywhere...
	//scionPrinter->printLog(IH,type,ts,src,m_uAdAid,"IFID REP: %d - %d (size) %u,RECEIVED\n",ifid, nifid, packetLength);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONBeaconServerCore)






