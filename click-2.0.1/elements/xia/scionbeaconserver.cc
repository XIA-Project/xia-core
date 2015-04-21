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
#include <polarssl/x509_crt.h>
#include <polarssl/pk.h>
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
#include <click/xiatransportheader.hh>
#include <click/xid.hh>
#include <click/standard/xiaxidinfo.hh>
#include "xiatransport.hh"
#include "xiaxidroutetable.hh"
#include "xtransport.hh"

#define SID_XROUTE  "SID:1110000000000000000000000000000000001112"

CLICK_DECLS

int SCIONBeaconServer::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile,
        cpEnd) <0){
            click_chatter("Fatal error: click configuration fail at SCIONBeaconServer.\n");
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

int SCIONBeaconServer::initialize(ErrorHandler* errh){

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
    config.getPCBLogFilename((char*)m_csLogFile);
    config.getROTFilename((char*)m_sROTFile);

    scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile, this->class_name());
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char*)"Initializes.\n");
    #endif

    m_iResetTime = config.getResetTime();
    m_iScheduleTime = SCIONCommonLib::GCD(m_iRegTime, m_iPropTime);
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char*)"PCB reg = %d, prop = %d, schedule time = %d\n", 
        m_iRegTime, m_iPropTime, m_iScheduleTime);
    #endif
    m_iIsRegister = config.getIsRegister(); 
    //whether to register paths to TDC or not
    m_iKval = config.getNumRegisterPath(); 
    //# of paths that can be registered to TDC
    m_iBeaconTableSize = config.getPCBQueueSize();
    
    time(&m_lastPropTime);
    m_lastRegTime = m_lastPropTime;
    
    // task 2: parse ROT (root of trust) file
    m_bROTInitiated = parseROT();
    if(m_bROTInitiated)
        #ifdef _DEBUG_BS
        scionPrinter->printLog(IH, (char*)"Parse/Verify ROT Done.\n");
        #endif
        
    // task 3: parse topology file
    parseTopology();
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char*)"Parse Topology Done.\n");
    #endif

    // task 4: read key pair if they already exist
    loadPrivateKey();
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char*)"Private Key Loading Done.\n");
    #endif
    
    // task 5: generate/update OFG key
    if(!initOfgKey()){
        click_chatter("Fatal error: Init OFG Key fail at BS.\n");
        exit(-1);
    }
    if(!updateOfgKey()){
        click_chatter("Fatal error: update OFG Key fail at BS.\n");
        exit(-1);
    }

    srand(time(NULL));
    
    //initialize per-child path selection policy
    initSelectionPolicy();
    
    _timer.initialize(this); 
    _timer.schedule_after_sec(10);
    
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char*)"Initialization Done.\n");
    #endif

    return 0;
}

bool SCIONBeaconServer::initOfgKey() {

    time_t currTime;
    time(&currTime);
    //SL: to deal with the case that TDC BS initiate PCB before this BS starts
    currTime -= 300; //back 5minutes
    m_currOfgKey.time = currTime;
    memcpy(m_currOfgKey.key, &m_uMkey, OFG_KEY_SIZE);
    
    int err;
    if(err = aes_setkey_enc(&m_currOfgKey.actx, m_currOfgKey.key, 
        OFG_KEY_SIZE_BITS)) {
        #ifdef _DEBUG_BS
	    scionPrinter->printLog(EH, (char *)"OFG Key setup failure.\n");
	    #endif
	    return SCION_FAILURE;
    }
    return SCION_SUCCESS;
}

bool SCIONBeaconServer::updateOfgKey() {
    
    //This function should be updated...
    m_prevOfgKey.time = m_currOfgKey.time;
    memcpy(m_prevOfgKey.key, m_currOfgKey.key, OFG_KEY_SIZE);
	
    int err;
    if(err = aes_setkey_enc(&m_prevOfgKey.actx, m_prevOfgKey.key, 
        OFG_KEY_SIZE_BITS)) {
	#ifdef _DEBUG_BS
        scionPrinter->printLog(EH, (char *)"Prev. OFG Key setup failure.\n");
        #endif
        return SCION_FAILURE;
    }

    return SCION_SUCCESS;
}

void SCIONBeaconServer::parseTopology(){
    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    // TODO: retrieve related topology information
    // form the controller
    parser.parseServers(m_servers);
    parser.parseEgressIngressPairs(m_routepairs);
}

bool SCIONBeaconServer::parseROT(char* loc){

    ROTParser parser;
    char* fn = NULL;
    ROT tROT;
    
    if(loc)
        fn = loc;
    else
        fn = m_sROTFile;
	
    if(parser.loadROTFile(fn)!=ROTParseNoError){
        #ifdef _DEBUG_BS
        scionPrinter->printLog(WH, (char *)"ROT missing.\n");
        #endif
        return SCION_FAILURE;
    }
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"Load ROT OK.\n");
    #endif
	
    if(parser.parse(tROT)!=ROTParseNoError){
        #ifdef _DEBUG_BS
        scionPrinter->printLog(WH, (char *)"ERR: ROT parsing error.\n");
        #endif
        return SCION_FAILURE;
    }
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"Parse ROT OK.\n");
    #endif
	
    if(parser.verifyROT(tROT)!=ROTParseNoError){
        #ifdef _DEBUG_BS
        scionPrinter->printLog(WH, (char *)"ROT verify error.\n");
        #endif
        return SCION_FAILURE;
    }
	
    //Store the ROT if verification passed.
    parser.parse(m_cROT);
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"Verify ROT OK.\n");
    #endif
    return SCION_SUCCESS;
}

void SCIONBeaconServer::loadPrivateKey() {
    if(!_CryptoIsReady) {
        rsa_init(&PriKey, RSA_PKCS_V15, 0);	
        int ret;
        pk_context pk;
        pk_init( &pk );
        ret = pk_parse_keyfile( &pk, m_csPrvKeyFile, NULL );
        if( ret == 0 && ! pk_can_do( &pk, POLARSSL_PK_RSA ) )
            ret = POLARSSL_ERR_PK_TYPE_MISMATCH;
        if( ret == 0 )
            rsa_copy( &PriKey, pk_rsa( pk ) );

        if( ret != 0) {
            rsa_free(&PriKey);
            pk_free(&pk);
            click_chatter("Fatal error: Private key file loading failure, path = %s\n", 
                (char*)m_csPrvKeyFile);
            exit(-1);
        }
        // check private key
        if(rsa_check_pubkey(&PriKey)!=0) {
            rsa_free(&PriKey);
            click_chatter("Fatal error: Private key is not well-formatted, path = %s\n", 
                (char*)m_csPrvKeyFile);
            exit(-1);
        }
        _CryptoIsReady = true;
    }
}

void SCIONBeaconServer::run_timer(Timer *timer){
    
    sendHello();
    
    time_t curTime;
    time(&curTime);
    
    // check if ROT file is missing
    if(!m_bROTInitiated) {
        #ifdef _DEBUG_BS
        scionPrinter->printLog(WH, (char *)"ROT is missing or wrong formatted.\n");
        #endif
        requestROT();
        
    }else{
        // time for registration
        if(m_iIsRegister && curTime - m_lastRegTime >= m_iRegTime) {
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
    }
    _timer.reschedule_after_sec(m_iScheduleTime);
}

void SCIONBeaconServer::push(int port, Packet *p)
{
    TransportHeader thdr(p);
    uint8_t* s_pkt = (uint8_t *)thdr.payload();
    uint16_t type = SPH::getType(s_pkt);
    uint16_t packetLength = SPH::getTotalLen(s_pkt);
    uint8_t packet[packetLength];
    memset(packet, 0, packetLength);
    memcpy(packet, s_pkt, packetLength);
    p->kill();
    
    switch(type)
    {
        case BEACON:
            {
                if(m_bROTInitiated){
                    uint32_t ROTVersion = SCIONBeaconLib::getROTver(packet);
                    #ifdef _DEBUG_BS
                    scionPrinter->printLog(IH, (char*)"Received Beacon with RoT ver = %d, self RoT ver = %d.\n", 
                        ROTVersion, m_cROT.version);
                    #endif
                    // ROT version handler
                    if(ROTVersion > m_cROT.version){
                        #ifdef _DEBUG_BS
                        scionPrinter->printLog(IH, (char*)"RoT version has been changed. Get a new ROT from local CS.\n");
                        #endif
			            requestROT(ROTVersion);
			        }else{
			            processPCB(packet, packetLength);
			        }
                }else{
                    #ifdef _DEBUG_BS
                    scionPrinter->printLog(WH, (char *)"No valid ROT file. Ignoring PCB.\n");
                    #endif
                }
            }
            break;
            
        case CERT_REP_LOCAL:
            saveCertificate(packet, packetLength);
            break;
        
        case ROT_REP_LOCAL:
            #ifdef _DEBUG_BS
            scionPrinter->printLog(IH, (char*)"Received ROT_REP_LOCAL from local CS.\n");
            #endif
            saveROT(packet, packetLength);
            break;
			
        default:
            break;
    }
}

void SCIONBeaconServer::requestROT(uint32_t version) {
	
    if(m_servers.find(CertificateServer)!=m_servers.end()){
        // Request ROT from Cert Server
        uint8_t hdrLen = COMMON_HEADER_SIZE+AIP_SIZE*2;
        uint16_t totalLen = hdrLen + sizeof(struct ROTRequest);
        uint8_t packet[totalLen];
	    
        scionHeader hdr;
        hdr.cmn.type = ROT_REQ_LOCAL;
        hdr.cmn.hdrLen = hdrLen;
        hdr.cmn.totalLen = totalLen;
        hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1));
        hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)(m_servers.find(CertificateServer)->second.HID));
        SPH::setHeader(packet, hdr);

    	// set default version number as 0
    	struct ROTRequest req;
    	req.previousVersion = m_cROT.version;
    	req.currentVersion = version; 
    	//currentVersion == 0 indicates the most recent version
    	*(ROTRequest *)(packet+hdrLen) = req;
 
    	// use explicit path instead of using destination (AD, HID) only
    	string dest = "RE ";
    	dest.append(" ");
    	dest.append(BHID);
    	dest.append(" ");
    	dest.append(m_AD.c_str());
        dest.append(" ");
    	// local cert server 
    	dest.append("HID:");
    	dest.append((const char*)m_servers.find(CertificateServer)->second.HID);

    	sendPacket(packet, totalLen, dest);

        #ifdef _DEBUG_BS
    	scionPrinter->printLog(WH, (char *)"ROT is missing or wrong formatted. \
Send ROT request to local cert server.\n");
        #endif
        
    }else{
        #ifdef _DEBUG_BS
        scionPrinter->printLog(EH, (char*)"AD (%s) does not has cert server.\n", m_AD.c_str());
        #endif 
    }
}

void SCIONBeaconServer::saveROT(SPacket * packet, uint16_t packetLength) {

    SPH::getHdrLen(packet);
    ROTRequest *req = (ROTRequest *)SPH::getData(packet);
    int curver = (int)req->currentVersion;

    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, SPH::getType(packet), (char *)"ROT Replay cV = %d, pV = %lu\n", 
        curver, m_cROT.version);
    #endif

    if(curver >= m_cROT.version) {
       
       //1.Store temp ROT file
       char nFile[MAX_FILE_LEN];
       memset(nFile, 0, MAX_FILE_LEN);
       sprintf(nFile,"./TD1/Non-TDC/AD%llu/beaconserver/ROT/rot-td%llu-%llu.xml", m_uAdAid, m_uTdAid, req->currentVersion);
       
       #ifdef _DEBUG_BS
       scionPrinter->printLog(IH, (char*)"Update ROT file: %s\n", nFile);
       #endif
       
       uint16_t offset = SPH::getHdrLen(packet)+sizeof(ROTRequest);
       uint16_t rotLen = packetLength-offset;
       // Write to file defined in Config
       
       FILE* rotFile = fopen(nFile, "w+");
       fwrite(packet+offset, 1, rotLen, rotFile);
       fclose(rotFile);
       
       //1.2. verify using ROTParser
       m_bROTInitiated = parseROT(nFile);
       if(!m_bROTInitiated) {
           #ifdef _DEBUG_BS
           scionPrinter->printLog(WH, (char *)"fails to parse received ROT.\n");
           #endif
           // remove file
           remove(nFile);
           return;
        }else{
            #ifdef _DEBUG_BS
            scionPrinter->printLog(IH, (char *)"stores received ROT.\n");
            #endif
            strncpy( m_sROTFile, nFile, MAX_FILE_LEN );
        }
    }else{
    	// downgrade, should be error or attacks!
    }
}

void SCIONBeaconServer::processPCB(uint8_t* packet, uint16_t packetLength){

	uint8_t srcLen = SPH::getSrcLen(packet);
	uint8_t dstLen = SPH::getDstLen(packet);
	uint32_t ts = SPH::getTimestamp(packet);
	HostAddr srcAddr = SPH::getSrcAddr(packet);
	HostAddr dstAddr = SPH::getDstAddr(packet);
	
	//PCB verification
	if(!verifyPcb(packet)){
	
	    #ifdef _DEBUG_BS
		scionPrinter->printLog(IH, ts, (char *)"PCB VERIFY FAIL.\n");
	    #endif

	}else{
	
	    #ifdef _DEBUG_BS
		scionPrinter->printLog(IH,ts,(char *)"PCB VERIFY PASS.\n");
		#endif
		
		//adds pcb to beacon table
		addPcb(packet);
		
		//send pcb to all path servers (for up-paths). 
		uint8_t exp = 0; //expiration time

		if(m_servers.find(PathServer)!=m_servers.end()){
			
			std::multimap<int, Servers>::iterator it;
			std::pair<std::multimap<int, Servers>::iterator,std::multimap<int,Servers>::iterator> pathServerRange;
			pathServerRange = m_servers.equal_range(PathServer);

			aes_context  actx;
			if(!getOfgKey(SPH::getTimestamp(packet),actx)) {
			    #ifdef _DEBUG_BS
				scionPrinter->printLog(EH,ts,(char *)"OFG KEY retrieval failure.\n");
				#endif
				return;
			}

			uint16_t ingress = SCIONBeaconLib::getInterface(packet);
			packetLength += PCB_MARKING_SIZE;

			//adds own marking to pcb
			uint8_t newPacket[packetLength];
			memset(newPacket, 0, packetLength);
			memcpy(newPacket, packet, packetLength-PCB_MARKING_SIZE);
			SCIONBeaconLib::addLink(newPacket, ingress, NON_PCB, 1, m_uAdAid, m_uTdAid, &actx, 0, exp, 0, (uint16_t)PriKey.len);

			//removes signature from pcb
			uint8_t path[packetLength]; 
			uint16_t pathLength = removeSignature(newPacket, path);
			uint8_t hdrLen = COMMON_HEADER_SIZE+srcLen+dstLen;
			
			memset(newPacket+hdrLen, 0, packetLength-hdrLen);
			memcpy(newPacket+hdrLen, path, pathLength);

			scionHeader hdr;
			hdr.cmn.type = UP_PATH;
			hdr.cmn.hdrLen = hdrLen;
			hdr.cmn.totalLen = hdrLen+pathLength;
			hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1));
			
			// send this packet to all path servers
			for(it=pathServerRange.first; it!=pathServerRange.second; it++) { 
				
				hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)(it->second.HID));
				SPH::setHeader(newPacket, hdr);
				
				string dest = "RE ";
				dest.append(BHID);
				dest.append(" ");
				dest.append(m_AD.c_str());
				dest.append(" ");
				dest.append("HID:");
				dest.append((const char*)it->second.HID);
				
				#ifdef _DEBUG_BS
				scionPrinter->printLog(IH, (char *)"Sending Up-Path packet to local PS.\n");
				#endif
				
				sendPacket(newPacket, packetLength, dest);
			}
			
		} else {
		    #ifdef _DEBUG_BS
			scionPrinter->printLog(EH, (char*)"AD (%s) does not has path server.\n", m_AD.c_str());
			#endif
		}
		
	}
}

void SCIONBeaconServer::saveCertificate(SPacket * packet, uint16_t packetLength) {
    uint8_t hdrLen = SPH::getHdrLen(packet);
    certInfo* info = (certInfo*)(packet+hdrLen);
    uint64_t target = info->target;
    uint16_t cLength = info->length;
	
    char cFileName[MAX_FILE_LEN]; 
    sprintf(cFileName,"./TD1/Non-TDC/AD%lu/beaconserver/certificates/td%u-ad%lu-0.crt", m_uAdAid, m_uTdAid, target);
    // erase from the request queue
    certRequest.erase(target);
    // store certificate
    FILE* cFile = fopen(cFileName,"w");
    fwrite(packet+hdrLen+CERT_INFO_SIZE,1,cLength,cFile);
    fclose(cFile);
	
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char*)"saves certificate at %s.\n", cFileName);
    #endif
}

uint16_t SCIONBeaconServer::removeSignature(SPacket* inPacket, uint8_t* path){

    uint16_t totalLength = SPH::getTotalLen(inPacket);
    uint16_t numHop = SCIONBeaconLib::getNumHops(inPacket);
    uint8_t buf[totalLength];
    memset(buf, 0, totalLength);
    uint8_t hdrLen = SPH::getHdrLen(inPacket);

    // buf has special OF + series of OFs
    // 1. copy the special opaque field (TS) to the buffer.
    memcpy(buf, inPacket+hdrLen, OPAQUE_FIELD_SIZE);

    uint8_t* ptr = inPacket+hdrLen+OPAQUE_FIELD_SIZE*2;
    pcbMarking* mrkPtr = (pcbMarking*)ptr;
    uint16_t offset = OPAQUE_FIELD_SIZE;

    // 2. copy markings excluding signature
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

void SCIONBeaconServer::addPcb(SPacket* pkt) {
	
	// if the beacon table is full then remove the oldest pcb
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
    
#ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"Insert PCB in beacon_table (%u, %d, %u).\n", 
        ts, newPcb.hops, newPcb.ingress);
#endif
    
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
	
    #ifdef _DEBUG_BS
	scionPrinter->printLog(IH, (char *)"Insert PCB in unverified_pcbs (%u, %d, %u).\n", 
	    ts, newPcb.hops, newPcb.ingress);
    #endif

	std::pair<uint32_t, pcb> npair = std::pair<uint32_t,pcb>(ts, newPcb);
	unverified_pcbs.insert(npair);
}

int SCIONBeaconServer::registerPaths() {
    if(beacon_table.empty()){
        return 0;
    }

    #ifdef _DEBUG_BS
	scionPrinter->printLog(IH, (char *)"registers path to TDC for %s. Beacon Table Size = %d\n", 
	    m_AD.c_str(), beacon_table.size());
	#endif

    // If it is already fully registered.
    if(m_iNumRegisteredPath >= m_iKval){ 
        //fully registered
		//if all k paths are already registered, removes 3 registered pcb (just for testing)
		//need to check path expiration time
		//just for test ---
		//if k paths are already registered, the BS replaces only the oldest one with a new fresh path
		//path replacement policy needs to be defined in more detail
        for(int i=0;i<3;i++){  
            k_paths.erase(k_paths.begin());
            m_iNumRegisteredPath--;
        }
    }

    // register up to k paths from the newest one in the beacon table 
	// Note: registered paths are not thrown away in order to compute "path fidelity."
    // if k paths are fully registered, then stop.
    
	//reverse iterator is better to find a best (fresh) path since the key is ordered by timestamp in the map
	std::multimap<uint32_t, pcb>::reverse_iterator itr;
	
	for(int i = m_iNumRegisteredPath; i < m_iKval; i++) {
		if(!getPathToRegister(itr)){
		    #ifdef _DEBUG_BS
    		scionPrinter->printLog(IH, PATH_REG, (char *)"%s: %i paths registered to TDC. No path to register.\n",
    		    m_AD.c_str(), m_iNumRegisteredPath);
    		#endif
			break;
		}
		registerPath(itr->second);
		// set as true for registered flag
		itr->second.registered = 1;
        m_iNumRegisteredPath++;
        k_paths.insert(std::pair<uint32_t, pcb>(itr->first, itr->second));
		// what if path registration failed at TDC for some unknown reason?
		// need to handle this as a special case in the future (consistency between BS and TDC PS)
	}
	
    return 0;
}

bool SCIONBeaconServer::getPathToRegister(std::multimap<uint32_t, pcb>::reverse_iterator &iter) {

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

void SCIONBeaconServer::printPaths(){
    
    std::multimap<uint32_t, pcb>::iterator itr; 
    for(itr=beacon_table.begin();itr!=beacon_table.end();itr++){
    	string path_info;
        uint8_t hdrLen = SPH::getHdrLen(itr->second.msg); 
        uint8_t* ptr = itr->second.msg+hdrLen+OPAQUE_FIELD_SIZE*2;
        pcbMarking* mrkPtr = (pcbMarking*)ptr;
        scionPrinter->printLog(IH, (char *)"Path info:");
        for(int i=0;i<itr->second.hops;i++){
            scionPrinter->printLog((char*)" %lu(%d:%d) | ", mrkPtr->aid,  mrkPtr->ingressIf, mrkPtr->egressIf);
            ptr+=mrkPtr->sigLen + mrkPtr->blkSize;
            mrkPtr = (pcbMarking*)ptr;
        }
        scionPrinter->printLog(IH, (char*)"%lu  %d\n",m_uAdAid, itr->second.registered);
    }
}

int SCIONBeaconServer::registerPath(pcb &rpcb){

    //1. construct a path by copying pcb to packet
    uint16_t packetLength = rpcb.totalLength+PCB_MARKING_SIZE;
    uint8_t packet[packetLength];
    memset(packet,0,packetLength);

    //2. original pcb received from the upstream
    memcpy(packet,rpcb.msg,rpcb.totalLength);

    aes_context actx;
    getOfgKey(SPH::getTimestamp(packet),actx); 

    //3. add its own marking information to the end
    //Expiration time needs to be configured by AD's policy
    uint8_t exp = 0; //expiration time
    uint16_t sigLen = PriKey.len;
    SCIONBeaconLib::addLink(packet, rpcb.ingress, NON_PCB, 1, m_uAdAid, m_uTdAid, &actx, 0, exp, 0, sigLen);

    //4. Remove signature from PCB
    //pathContent would get special OF (TS) (without 2nd special OF) + series of markings from TDC to itself
    //this would be registered to PS in TDC
    uint8_t pathContent[packetLength];
    memset(pathContent,0,packetLength);
    uint16_t pathContentLength = removeSignature(packet, pathContent);
    
    // Build path (list of OF) from the pcb
    // add 1 for special OF (i.e., timestamp)
    // 5. Now construct a path to the TDC (i.e., a series of OFs to the TDC)
    uint16_t numHop = SCIONBeaconLib::getNumHops(packet)+1; 
    
    //6. at this point we have "path" (TS+OFs to TDC) and "pathContent" that would be registered to TDC PS.
    uint8_t srcLen = SPH::getSrcLen(packet);
    uint8_t dstLen = SPH::getDstLen(packet);

    //7. now hdr for path registration include COMMON_HDR + SRC/DST Addr
    uint8_t hdrLen = COMMON_HEADER_SIZE+srcLen+dstLen;

    // create PATH_REG packet, including header and all OPAQUE FIELDS
    uint16_t newPacketLength = hdrLen+pathContentLength;
    uint8_t newPacket[newPacketLength];

    //set common header
    scionHeader hdr;
    hdr.cmn.type = PATH_REG;
    hdr.cmn.hdrLen = hdrLen;
    hdr.cmn.totalLen = newPacketLength;
    //set src/dst addresses
    hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1));
    hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)(m_servers.find(PathServer)->second.HID));
    SPH::setHeader(newPacket, hdr);
    memcpy(newPacket+hdrLen, pathContent, pathContentLength);
    
    char buf[MAXLINELEN];
    uint16_t offset = 0;
    uint8_t* ptr = pathContent+OPAQUE_FIELD_SIZE;
    pcbMarking* mrkPtr = (pcbMarking*)ptr;

    uint64_t tdc_ad = 0;
    for(int i=0;i<numHop-1;i++){
        if(i==0) tdc_ad = mrkPtr->aid;
        sprintf(buf+offset,"%lu (%u, %u) |", mrkPtr->aid, mrkPtr->ingressIf, mrkPtr->egressIf);
        ptr+=mrkPtr->blkSize;
        mrkPtr=(pcbMarking*)ptr;
        offset = strlen(buf);
    }
     
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, PATH_REG, rpcb.timestamp, (char *)"%s %u, SENT PATH: %s\n", m_AD.c_str(), newPacketLength, buf);
    #endif
	
    // TODO: remove hacked conversion for AD number to AD identifier
    char adbuf[AIP_SIZE+1];
    sprintf(adbuf, "%040llu", tdc_ad);

    string dest = "RE ";
    dest.append(BHID);
    dest.append(" ");
    dest.append("AD:");
    dest.append(adbuf);
    dest.append(" ");
    dest.append("HID:");
    dest.append((const char*)m_servers.find(PathServer)->second.HID);
    
    sendPacket(newPacket, newPacketLength, dest);
	
    return 0;
}

int SCIONBeaconServer::buildPath(SPacket* pkt, uint8_t* output){
	
	uint8_t hdrLen = SPH::getHdrLen(pkt);
	// TOS OF
	memcpy(output, pkt+hdrLen, OPAQUE_FIELD_SIZE);

    //after 2 special OFs, AD marking comes.
	uint8_t* ptr = pkt + hdrLen+OPAQUE_FIELD_SIZE*2; 
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

int SCIONBeaconServer::propagate() {	
	
	// Nothing should propagate 
	if(beacon_table.empty()) {
		return 0;
	}
	
	//iterators
	std::multimap<int, EgressIngressPair>::iterator pItr; //child routers iterators
	std::multimap<int, EgressIngressPair>::iterator cItr; //child routers iterators

	// find all the peer routers
	std::pair<std::multimap<int, EgressIngressPair>::iterator,
	std::multimap<int,EgressIngressPair>::iterator> peerRange;
	peerRange = m_routepairs.equal_range(Peer);

	// find child routers
	std::pair<std::multimap<int, EgressIngressPair>::iterator,
	std::multimap<int,EgressIngressPair>::iterator> childRange;
	childRange = m_routepairs.equal_range(Child);
	
	// Nothing should propagate 
	if(m_routepairs.count(Child)==0) 
	{
	    #ifdef _DEBUG_BS
		scionPrinter->printLog(IH, (char*)"BS(%s) is stub AD.\n", m_HID.c_str());
		#endif
		return 0;
	}
	
	//SL: Expiration time needs to be configured by AD's policy
	uint8_t exp = 0; //expiration time
	//HC: pcb->propagated, pcb->age can be removed, as each customer
	//has a different set of selected paths
    
    for(cItr=childRange.first;cItr!=childRange.second;cItr++){

		EgressIngressPair rpair = cItr->second;
		
		uint16_t ifid = (uint16_t)strtoull((const char*)rpair.egress_addr, NULL, 10);
		std::map<uint16_t, SelectionPolicy>::iterator curPolicyIt = m_selPolicies.find(ifid);
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

			aes_context actx;
			//SLT:TS
			//HC: why can't we get timestamp from pcb->timestamp directly?
			getOfgKey(SPH::getTimestamp(msg),actx);
			
			uint16_t sigLen = PriKey.len;
			uint16_t egress_id = (uint16_t)strtoull((const char*)rpair.egress_addr, NULL, 10);
			SCIONBeaconLib::addLink(msg, curPcb->ingress, egress_id, PCB_TYPE_TRANSIT, m_uAdAid, m_uTdAid, &actx,0,exp,0, sigLen);
 
			SPH::setDstAddr(msg, HostAddr(HOST_ADDR_AIP,(uint8_t*)rpair.egress_addr) );
			SCIONBeaconLib::setInterface(msg, (uint64_t)strtoull((const char*)rpair.ingress_addr, NULL, 10));
			
			uint64_t next_aid = strtoull((const char*)rpair.ingress_ad, NULL, 10);
			SCIONBeaconLib::signPacket(msg, PriKey.len, next_aid, &PriKey);
			uint16_t msgLength = SPH::getTotalLen(msg);
			
			//SLT: TS
			uint32_t ts = SPH::getTimestamp(msg);
			#ifdef _SL_DEBUG_BS
			scionPrinter->printLog(IH,BEACON,ts,(char *)m_AD.c_str(),(uint64_t)strtoull((const char*)rpair.ingress_ad, NULL, 10),(char *)"%u,SENT\n",msgLength);
			#endif
			
			string dest = "RE ";
			dest.append(BHID);
			dest.append(" ");
			// egress router
			dest.append("HID:");
			dest.append((const char*)rpair.egress_addr);
			// destination AD
			dest.append(" ");
			dest.append("AD:");
			dest.append((const char*)rpair.ingress_ad);
			// ingress router
			dest.append(" ");
			dest.append("HID:");
			dest.append((const char*)rpair.ingress_addr);
			// destination beacon server
			dest.append(" ");
			dest.append("HID:");
			dest.append((const char*)rpair.dest_addr);
			
			#ifdef _DEBUG_BS
			scionPrinter->printLog(IH, (char*)"propagate PCB to AD %s.\n", rpair.dest_ad);
			#endif
			
            sendPacket(msg, msgLength, dest);
			
			nPropagated++;
			if(nPropagated >= m_iKval) break;
		}
    }
	
    return SCION_SUCCESS;
}

uint8_t SCIONBeaconServer::verifyPcb(SPacket* pkt){
	
    uint8_t hdrLen = SPH::getHdrLen(pkt);
    pcbMarking* mrkPtr = (pcbMarking*)(pkt+hdrLen+OPAQUE_FIELD_SIZE*2);
    uint8_t* ptr = (uint8_t*)mrkPtr;
    uint8_t hops = SCIONBeaconLib::getNumHops(pkt); 

	uint16_t sigLen = mrkPtr->sigLen;
	uint16_t msgLen = mrkPtr->blkSize;
	uint8_t srcLen = SPH::getSrcLen(pkt);
	uint8_t dstLen = SPH::getDstLen(pkt);

	// signature
	uint8_t sig[sigLen];
	memcpy(sig, ptr+mrkPtr->blkSize, sigLen);
	
	// marking (Opaque Field + marking info)
	// add next AID (8Bytes) 
	uint8_t msg[msgLen+SIZE_AID];
	memset(msg, 0, msgLen);
	memcpy(msg, ptr, msgLen);
	pcbMarking* nextPtr;
	
	// chaining next AID (i.e., my AID)
	if(hops > 1) { 
		//if more than one signatures exist
		nextPtr = (pcbMarking*)(ptr + msgLen + sigLen);
		memcpy(&msg[msgLen], &(nextPtr->aid), SIZE_AID);
	} else {
		memcpy(&msg[msgLen], &m_uAdAid, SIZE_AID);
	}
	msgLen += SIZE_AID;

	// Part1: TD Core Verification, Verify Using ROT structure
	x509_crt TDCert;
	int ret = 0;
	memset(&TDCert, 0, sizeof(x509_crt));
	ret = x509_crt_parse(&TDCert, 
	    (const unsigned char*)m_cROT.coreADs.find(mrkPtr->aid)->second.certificate,
		m_cROT.coreADs.find(mrkPtr->aid)->second.certLen);
		
	// certificate not exist yet
	if( ret < 0 ) {
	    #ifdef _DEBUG_BS
		scionPrinter->printLog(EH, (char*)"x509parse_crt parsing failed.\n");
		#endif
		x509_crt_free( &TDCert );
		// Add it to Queue
		addUnverifiedPcb(pkt);
		return SCION_FAILURE;
	}

	if(SCIONCryptoLib::verifySig(msg, sig, msgLen, pk_rsa(TDCert.pk)) != scionCryptoSuccess) {
	    #ifdef _DEBUG_BS
		scionPrinter->printLog(EH, (char*)"Signature verification (TDC) failure.\n");
		#endif
		x509_crt_free(&TDCert);
        return SCION_FAILURE;
	}else{
		x509_crt_free( &TDCert );
	}
	
	// Non TDC signature verification
	uint8_t cert[MAX_FILE_LEN];
	x509_crt * adcert = NULL;
	std::map<uint64_t,x509_crt*>::iterator it;
	
	pcbMarking* prevMrkPtr = mrkPtr;
	ptr+=mrkPtr->blkSize+mrkPtr->sigLen;
	mrkPtr = (pcbMarking*)ptr;
	
	for(int i=1;i<hops;i++){
		
		if(mrkPtr->aid == m_uAdAid) {
		    #ifdef _DEBUG_BS
			scionPrinter->printLog(EH, (char*)"Duplicate error possibly by a NIC driver.\n");
			#endif
			return SCION_FAILURE;
		}

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
			    #ifdef _DEBUG_BS
				scionPrinter->printLog(WH, (char*)"Certificate of AD %lu does not exist.\n", mrkPtr->aid);
				#endif
				addUnverifiedPcb(pkt);
				requestForCert(pkt); 
				return SCION_FAILURE;
			}
			fclose(cFile);

			adcert = new x509_crt;
			memset(adcert,0,sizeof(x509_crt));

			int err = x509_crt_parse_file(adcert, (const char*)cert);
			if(err){
			    #ifdef _DEBUG_BS
				scionPrinter->printLog(EH, (char*)"fail to extract AD %lu's cert to verify signatures.\n", mrkPtr->aid);
				#endif
				x509_crt_free(adcert);
				delete adcert;
				return SCION_FAILURE;		
			}
			m_certMap.insert(std::pair<uint64_t,x509_crt*>(mrkPtr->aid, adcert));
		}
		else {
			adcert = it->second;
		}
		
		// verify this signature
		// now subChain includes all certificates
		// need to verify certificate and cache the last one (do this once in a predefined period)
		// use x509parse_verify, Note: use "x509_crt->next"
		if(SCIONCryptoLib::verifySig(content,signature,msgLen+SIZE_AID,pk_rsa(adcert->pk))!=scionCryptoSuccess){
		    #ifdef _DEBUG_BS
			scionPrinter->printLog(EH, (char*)"Signature verification failed for AD %lu\n", mrkPtr->aid);
			#endif
			x509_crt_free(adcert);
			return SCION_FAILURE;
		}
		
		prevMrkPtr = mrkPtr;
		ptr += (mrkPtr->blkSize + mrkPtr->sigLen);
		mrkPtr = (pcbMarking*)ptr;
	}

	return SCION_SUCCESS;
}

bool SCIONBeaconServer::getOfgKey(uint32_t timestamp, aes_context &actx) {
	if(timestamp > m_currOfgKey.time) {
		actx = m_currOfgKey.actx;
	} else {
		actx = m_prevOfgKey.actx;
	}
    return SCION_SUCCESS;
}

void SCIONBeaconServer::sendHello() {
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

void SCIONBeaconServer::sendPacket(uint8_t* data, uint16_t data_length, string dest) {

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

void SCIONBeaconServer::getCertFile(uint8_t* fn, uint64_t target){
    sprintf((char*)fn,"./TD1/Non-TDC/AD%lu/beaconserver/certificates/td%u-ad%lu-0.crt",
    m_uAdAid, m_uTdAid, target); 
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
}

void SCIONBeaconServer::requestForCert(SPacket* pkt){

    if(m_servers.find(CertificateServer)!=m_servers.end()){
	    
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
	    
	    //Add its own opaque field
	    aes_context actx;
	    getOfgKey(SPH::getTimestamp(pkt),actx);
	    
	    uint16_t tmpPacketLength = SPH::getTotalLen(pkt);
	    tmpPacketLength += PCB_MARKING_SIZE; //reserve space for an additional marking
	    uint8_t tmpPacket[tmpPacketLength];  //for copying the original packet to the increased buffer
	    memcpy(tmpPacket,pkt,tmpPacketLength-PCB_MARKING_SIZE);
	    
	    uint16_t ingIf = SCIONBeaconLib::getInterface(pkt);
	    SCIONBeaconLib::addLink(tmpPacket, ingIf, NON_PCB, 1, m_uAdAid, m_uTdAid, &actx, 0, 0, 0, (uint16_t)PriKey.len);
	    
	    hops = hops+1;
	    
	    //additional OF for itself since PCB has not been marked yet
	    uint8_t path[OPAQUE_FIELD_SIZE*hops];
	    buildPath(tmpPacket, path);
	    
	    hdrLen = COMMON_HEADER_SIZE+AIP_SIZE*2+hops*OPAQUE_FIELD_SIZE;
	    uint8_t packetLength = hdrLen+CERT_REQ_SIZE;
	    
	    //build certificate request packet
	    uint8_t newPacket[packetLength];
	    
	    scionHeader hdr;
	    //set common header
	    hdr.cmn.type = CERT_REQ_LOCAL;
	    hdr.cmn.hdrLen = hdrLen;
	    hdr.cmn.totalLen = packetLength;
	    
	    //set src/dst addresses
	    hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)(strchr(m_HID.c_str(),':')+1));
	    hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)(m_servers.find(CertificateServer)->second.HID));
	    
	    //set opaque fields
	    hdr.n_of = hops;
	    hdr.p_of = path;
	    
	    SPH::setHeader(newPacket,hdr);
	    
	    // copy CertReq Structure
	    memcpy(newPacket+hdrLen, &newReq, CERT_REQ_SIZE);
 
    	// use explicit path instead of using destination (AD, HID) only
    	string dest = "RE ";
    	dest.append(" ");
    	dest.append(BHID);
    	dest.append(" ");
    	dest.append(m_AD.c_str());
		dest.append(" ");
    	// local cert server 
    	dest.append("HID:");
    	dest.append((const char*)m_servers.find(CertificateServer)->second.HID);

    	sendPacket(newPacket, packetLength, dest);

        #ifdef _DEBUG_BS
    	scionPrinter->printLog(IH, (char *)"Request Certs from local CS.\n");
        #endif
        
	}else{
	    #ifdef _DEBUG_BS
    	scionPrinter->printLog(EH, (char*)"AD (%s) does not has cert server.\n", 
    	    m_AD.c_str());
    	#endif 
	}
	
}

void SCIONBeaconServer::clearCertificateMap() {
    std::map<uint64_t, x509_crt*>::iterator it;
    x509_crt * pChain;
    for(it=m_certMap.begin(); it!=m_certMap.end(); it++){
        pChain = it->second;
        x509_crt_free(pChain);
        delete pChain;
    }
    m_certMap.clear();
}

void SCIONBeaconServer::clearAesKeyMap() {
    std::map<uint32_t, aes_context*>::iterator it;
    aes_context * actx;
    for(it=m_OfgAesCtx.begin(); it!=m_OfgAesCtx.end(); it++){
        actx = it->second;
        delete actx;
    }
    m_OfgAesCtx.clear();
}


// HC: 
// TODO: load policies from configuration
// Currently assign a simple policy for every child for testing purpose 
void SCIONBeaconServer::initSelectionPolicy(){

    std::multimap<int, EgressIngressPair>::iterator cItr; //child routers iterators

	// find all the peer routers
	std::pair<std::multimap<int, EgressIngressPair>::iterator,
	std::multimap<int,EgressIngressPair>::iterator> childRange;
	childRange = m_routepairs.equal_range(Child);

    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"numChildren=%d k=%d beacon_table_size=%d\n", 
    	m_routepairs.count(Child), m_iKval, m_iBeaconTableSize);    
    #endif

    for(cItr = childRange.first; cItr != childRange.second; ++cItr){
        EgressIngressPair rpair = cItr->second;
        SelectionPolicy tmpPl(m_iKval);
        //randomly selected parameters for testing purpose
        tmpPl.setExclusion(2000, 8, 100, std::tr1::unordered_set<uint64_t>());
        tmpPl.setWeight(0.4, 0.4, 0.2);
        tmpPl.setSelectionProbability(0.9);
        uint16_t ifid = (uint16_t)strtoull((const char*)rpair.egress_addr, NULL, 10);
        m_selPolicies.insert(std::pair<uint16_t, SelectionPolicy>(ifid, tmpPl) );
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
    click_chatter("computePriority: age=%d hops=%d normElapsed=%f priorityValue=%f\n", 
    curTime - candidate.timestamp, candidate.hops, normFresh, priValue);
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
    click_chatter("SelectionPolicy select: curTime %d\n", curTime);
    #endif

    std::vector<pcb*> selectedPcbs;
    std::priority_queue<PathPriority, std::deque<PathPriority> > pcbPriQueue;
    std::multimap<uint32_t, pcb>::const_reverse_iterator itr;
    std::tr1::unordered_set<scionHash, mySetHash, mySetEqual> duplicateFilter;

    int i = 0;
    for(itr = availablePaths.rbegin(); itr != availablePaths.rend(); ++itr){
    
        #ifdef _HC_DEBUG_BS
        click_chatter("HC_DEBUG: beacon_table line=%d, curTime=%d, ts=%d\n", i++, curTime, itr->first);
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
    #ifdef _DEBUG_BS
    click_chatter("timestamp: %u hops: %u | ", p->timestamp, p->hops);
    #endif
    for(size_t i = 0; i < p->hops; ++i){
        #ifdef _DEBUG_BS
        click_chatter("%lu(%d:%d) | ", mrkPtr->aid,  mrkPtr->ingressIf, mrkPtr->egressIf);
        #endif
        ptr+=mrkPtr->sigLen + mrkPtr->blkSize;
        mrkPtr = (pcbMarking*)ptr;
    }
    click_chatter("\n");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONBeaconServer)

