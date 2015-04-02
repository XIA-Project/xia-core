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

#include "scionbeaconserver_core.hh"
#include "scionprint.hh"
#include "rot_parser.hh"

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

int SCIONBeaconServerCore::configure(Vector<String> &conf, ErrorHandler *errh) {
    if(cp_va_kparse(conf, this, errh,
        "AD", cpkM, cpString, &m_AD,
        "HID", cpkM, cpString, &m_HID,
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile, 
        "TOPOLOGY_FILE",cpkM, cpString, &m_sTopologyFile,
        cpEnd) <0) {
            click_chatter("ERR: click configuration fail at SCIONBeaconServerCore.\n");
            click_chatter("ERR: Fault error, exit SCION Network.\n");
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

int SCIONBeaconServerCore::initialize(ErrorHandler* errh) {
    // task1: Config object get ROT, certificate file, private key path info
    Config config; 
    config.parseConfigFile((char*)m_sConfigFile.c_str());
    config.getPrivateKeyFilename((char*)m_csPrvKeyFile);
    config.getCertFilename((char*)m_csCertFile);
    config.getROTFilename((char*)m_sROTFile);
    config.getOfgmKey((char*)m_uMkey);
    config.getPCBLogFilename(m_csLogFile);

    // get AID, ADAID, TDID
    m_uAdAid = config.getAdAid();
    m_uTdAid = config.getTdAid();
    m_iLogLevel = config.getLogLevel();
    m_iPCBGenPeriod = config.getPCBGenPeriod();

    // setup looger, scionPrinter
    scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile); 
    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s) INIT.\n", m_AD.c_str(), m_HID.c_str());

    // task 2: parse ROT (root of trust) file
    m_bROTInitiated = parseROT();
    if(m_bROTInitiated)
        scionPrinter->printLog(IH, (char *)"Parse/Verify ROT Done.\n");

    // task 3: parse topology file
    parseTopology();
    scionPrinter->printLog(IH, (char *)"Parse Topology Done.\n");

    // task 4: read key pair if they already exist
    loadPrivateKey();
    scionPrinter->printLog(IH, (char *)"Load Private Key Done.\n");

    // for OFG fields
    if(!initOfgKey()){
        scionPrinter->printLog(EH, (char *)"Init OFG key failure.\n");
        click_chatter("ERR: InitOFGKey fail at SCIONBeaconServerCore.\n");
        click_chatter("ERR: Fatal error, exit SCION Network.\n");
        exit(-1);
    }
    if(!updateOfgKey()){
        scionPrinter->printLog(IH, (char *)"Update OFG key failure.\n");
        click_chatter("ERR: updateOFGKey fail at SCIONBeaconServerCore.\n");
        click_chatter("ERR: Fatal error, exit SCION Network.\n");
        exit(-1);
    }
    scionPrinter->printLog(IH, (char *)"Load OFG key Done.\n");
    scionPrinter->printLog(IH, (char *)"numChildren=%d\n", m_routepairs.count(Child));
    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s) INIT Done.\n", m_AD.c_str(), m_HID.c_str());

    // start scheduler to received packets
    ScheduleInfo::initialize_task(this, &_task, errh);
    // Trigger Timer
    _timer.initialize(this);
    //start after other elements are initialized
    //this part should be considered more carefully later.
    //e.g., how to handle PCBs propagated before others set up (e.g., ifid config)
    _timer.schedule_after_sec(10);
    return 0;
}

bool SCIONBeaconServerCore::parseROT(){
    ROTParser parser;
    if(parser.loadROTFile(m_sROTFile)!=ROTParseNoError){
        scionPrinter->printLog(EH, (char *)"ERR: ROT File missing at SCIONBeaconServerCore.\n");
        return SCION_FAILURE;
    }else{
        // ROT found in local folder
        if(parser.parse(m_cROT)!=ROTParseNoError){
            scionPrinter->printLog(EH, (char *)"ERR: ROT File parsing error at SCIONBeaconServerCore.\n");
            return SCION_FAILURE;
        }

        if(parser.verifyROT(m_cROT)!=ROTParseNoError){
            scionPrinter->printLog(EH, (char *)"ERR: ROT File parsing error at SCIONBeaconServerCore.\n");
            return SCION_FAILURE;
        }
    }
    return SCION_SUCCESS;
}

void SCIONBeaconServerCore::parseTopology(){
    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
    parser.parseEgressIngressPairs(m_routepairs);
}

void SCIONBeaconServerCore::loadPrivateKey() {
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
            pk_free( &pk );
            click_chatter("ERR:Private key file loading failure, path = %s", m_csPrvKeyFile);
            click_chatter("ERR: Fatal error. Exit SCION Network.");
            exit(-1);
        }
        if(rsa_check_privkey(&PriKey)!=0) {
            rsa_free(&PriKey);
            click_chatter("ERR: Private key is not well-formatted, path = %s", m_csPrvKeyFile);
            click_chatter("ERR: Fatal error. Exit SCION Network.");
            exit(-1);
        }
        _CryptoIsReady = true;
    }
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
        scionPrinter->printLog(EH, (char *)"OFG Key setup failure: %d\n",err*-1);
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
        scionPrinter->printLog(EH, (char *)"Prev. OFG Key setup failure: %d\n",err*-1);
        return SCION_FAILURE;
    }

    return SCION_SUCCESS;
}

/*
    SCIONBeaconServerCore::getOfgKey
    - finds OFG key for this timestamp.
    - If it does not exsists then creates new key and stores. 
*/
bool SCIONBeaconServerCore::getOfgKey(uint32_t timestamp, aes_context &actx)
{
    if(timestamp > m_currOfgKey.time) {
        actx = m_currOfgKey.actx;
    } else {
        actx = m_prevOfgKey.actx;
    }
    return SCION_SUCCESS;
    
    /*
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
            scionPrinter->printLog(EH, (char *)"OFG Key setup failure: %d\n",err*-1);
            return SCION_FAILURE;
        }

        m_OfgAesCtx.insert(std::pair<uint32_t, aes_context*>(timestamp, pActx));
        actx = *m_OfgAesCtx.find(timestamp)->second;
    }else{
        actx = *m_OfgAesCtx.find(timestamp)->second;
    }

    return SCION_SUCCESS;
    */
}

void SCIONBeaconServerCore::push(int port, Packet *p)
{
    TransportHeader thdr(p);
    
    uint8_t *s_pkt = (uint8_t *) p->data();
    //copy packet data and kills click packet
    uint16_t packetLength = SPH::getTotalLen(s_pkt);
    uint8_t srcLen = SPH::getSrcLen(s_pkt);
    uint8_t dstLen = SPH::getDstLen(s_pkt);
    uint8_t packet[packetLength];
    memset(packet, 0, packetLength);
    memcpy(packet, s_pkt, packetLength);
    p->kill();
    
    uint16_t type = SPH::getType(packet);
    uint32_t ts = SPH::getTimestamp(packet);

    switch(type) {
        case ROT_REP_LOCAL:
        {
            // open a file with 0 size
            int RotL = packetLength-(COMMON_HEADER_SIZE+srcLen+dstLen);
            scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s): Received ROT file size = %d\n", m_AD.c_str(), m_HID.c_str(), RotL);
            
            // Write to file defined in Config
            /*
            if(RotL>0)
            {
    		    FILE * rotFile = fopen(m_sROTFile, "w+");
    		    fwrite(packet+COMMON_HEADER_SIZE+srcLen+dstLen, 1, RotL, rotFile);
    		    fclose(rotFile);
    		    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s) stored received ROT.\n", m_AD.c_str(), m_HID.c_str());
    		    // try to verify local ROT again
    		    m_bROTInitiated = parseROT();
    		}
    		*/
    	}
    		break;
        default:
        	break;
    }
}

bool SCIONBeaconServerCore::run_task(Task *task) {
    _task.fast_reschedule();
    return true;
}


bool SCIONBeaconServerCore::generateNewPCB() {

    //Create PCB for propagation
    // get private key length
    uint16_t sigLen = PriKey.len;
    //get current time for timestamp
    timeval tv;
    gettimeofday(&tv, NULL); //use time for lower precision

    //create beacon
    uint8_t hdrLen = COMMON_HEADER_SIZE+SCION_ADDR_SIZE*2;
    uint16_t totalLen =hdrLen+OPAQUE_FIELD_SIZE*2+PCB_MARKING_SIZE+sigLen;
    uint8_t buf[totalLen];
    memset(buf, 0, totalLen); //empty scion header
     
    scionHeader hdr;
     
    // For XIA, address translation
    const char* format_str = strchr(m_HID.c_str(),':');
    HostAddr srcAddr = HostAddr(HOST_ADDR_SCION,(uint64_t)strtoull(format_str, NULL, 10)); //put HID as a source address
    HostAddr dstAddr = HostAddr(HOST_ADDR_SCION,(uint64_t)0); //destination address will be set to egress router's address
    // end of patch
     
    hdr.cmn.type = BEACON;
    hdr.cmn.hdrLen = COMMON_HEADER_SIZE+srcAddr.getLength()+dstAddr.getLength();
    hdr.cmn.totalLen = hdrLen+OPAQUE_FIELD_SIZE*2; //two opaque fields are used by TDC AD
     
    hdr.src = srcAddr;
    hdr.dst = dstAddr;
     
    SPH::setHeader(buf,hdr);
     
    #ifdef _SL_DEBUG_BS
    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s): InitBeacon: ROTVer:%d\n", m_AD.c_str(), m_HID.c_str(), m_cROT.version);
    #endif
    SCIONBeaconLib::initBeaconInfo(buf,tv.tv_sec,m_uTdAid,m_cROT.version); //SL: {packet,timestamp,tdid,ROT version} 
     
    #ifdef _SL_DEBUG_BS
    scionPrinter->printLog(IH, (char *)"PCB Header is ready\n");
    #endif
     
    // TimeStamp OFG field
    specialOpaqueField sOF = {SPECIAL_OF,tv.tv_sec,m_uTdAid,0}; //SLT: {type, timestamp, tdid, hops}
    SPH::setTimestampOF(buf, sOF);
    #ifdef _SL_DEBUG_BS
    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s): timestamp = %d, getTimestamp = %d, total Len = %d\n", m_AD.c_str(), m_HID.c_str(), tv.tv_sec, 
    SPH::getTimestamp(buf), SPH::getTotalLen(buf));
    #endif
     
    //get the OFG key that corresponds to the current timestamp and create aes context (i.e., key schedule)
    aes_context actx;
    if(!getOfgKey(tv.tv_sec, actx)) {
        //OFG key retrieval failure.
        scionPrinter->printLog(EH, (char *)"TDC BS (%s:%s): fatal error, fail to get ofg key.\n", m_AD.c_str(), m_HID.c_str());
        return SCION_FAILURE; //SL: need to define more specific error type
    }
    
    //iterators
    std::multimap<int, EgressIngressPair>::iterator cItr; //child routers iterators
    
    //find child routers
    std::pair<std::multimap<int, EgressIngressPair>::iterator,
    std::multimap<int,EgressIngressPair>::iterator> childRange;
    childRange = m_routepairs.equal_range(Child);
    
    // TimeStamp OFG field
    int cCount = 0;
    
    //iterate for all child routers
    for(cItr=childRange.first;cItr!=childRange.second;cItr++){
    
        EgressIngressPair rpair = cItr->second;
        cCount++;
        uint8_t msg[MAX_PKT_LEN];
        memset(msg, 0, MAX_PKT_LEN);
        memcpy(msg, buf, SPH::getTotalLen(buf));
        
        uint8_t exp = 0; //expiration time, default = non-expired
        
        // For XIA, address translation
        uint16_t egress_id = (uint16_t)strtoull((const char*)rpair.egress_addr, NULL, 10);
        uint16_t ingress_id = (uint16_t)strtoull((const char*)rpair.ingress_addr, NULL, 10);
        SCIONBeaconLib::addLink(msg, 0, egress_id, PCB_TYPE_CORE, m_uAdAid, m_uTdAid, &actx, 0, exp, 0, sigLen);
        SPH::setDstAddr(msg, HostAddr(HOST_ADDR_SCION,(uint64_t)strtoull((const char*)rpair.egress_addr, NULL, 10)));
        // xia does not have interface id yet..
        SCIONBeaconLib::setInterface(msg, (uint16_t)strtoull((const char*)rpair.ingress_addr, NULL, 10));
        //sign the above marking
        uint64_t next_aid = strtoull((const char*)rpair.ingress_ad, NULL, 10);
        // end of patch
        
        SCIONBeaconLib::signPacket(msg, sigLen, next_aid, &PriKey);
        uint16_t msgLength = SPH::getTotalLen(msg);
        
        scionPrinter->printLog(IH,BEACON,tv.tv_sec,1,(uint64_t)strtoull((const char*)rpair.ingress_ad, NULL, 10),(char *)"%u,SENT\n",msgLength);
        
        // use explicit path instead of using destination (AD, HID) only
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
        
        sendPacket(msg, msgLength, dest);
  }
  
  #ifdef _SL_DEBUG_BS
  scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s): childRange = %d\n", m_AD.c_str(), m_HID.c_str(), cCount);
  #endif

}

void SCIONBeaconServerCore::sendHello() {
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
    SCIONBeaconServerCore::run_timer
    Periodically generate PCB
*/
void SCIONBeaconServerCore::run_timer(Timer *){
	
	sendHello();
	
    if(m_bROTInitiated&&_CryptoIsReady){
        // ROT file is ready and private key is ready
        #ifdef _SL_DEBUG_BS
        scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s): Generate a New PCB.\n", m_AD.c_str(), m_HID.c_str());
        #endif
        generateNewPCB();

    	#ifdef _SL_DEBUG_BS
    	scionPrinter->printLog(IH, (char *)"PCB gen rescheduled in %d sec\n", m_iPCBGenPeriod);
    	#endif
    	
    	_timer.reschedule_after_sec(m_iPCBGenPeriod);
    
    }else if(!m_bROTInitiated){
    
    	// Request ROT from Cert Server
    	#ifdef _SL_DEBUG_BS
    	scionPrinter->printLog(EH, (char *)"TDC BS (%s:%s): ROT is missing or wrong formatted.\n", m_AD.c_str(), m_HID.c_str());
    	scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s): Send ROT Request to SCIONCertServerCore.\n", m_AD.c_str(), m_HID.c_str());
    	#endif
    	
    	const char* format_str = strchr(m_HID.c_str(),':');
    	HostAddr srcAddr = HostAddr(HOST_ADDR_SCION,(uint64_t)strtoull(format_str, NULL, 10)); 
    	//put HID as a source address
    	HostAddr dstAddr = HostAddr(HOST_ADDR_SCION,(uint64_t)strtoull((const char*)m_servers.find(CertificateServer)->second.HID, NULL, 10)); 
    	//destination address should be CS HID
    	
    	uint8_t hdrLen = COMMON_HEADER_SIZE+srcAddr.getLength()+dstAddr.getLength();
    	uint16_t totalLen = hdrLen + ROT_VERSION_SIZE;
    	uint8_t packet[totalLen];
    	
    	scionHeader hdr;
    	
    	hdr.cmn.type = ROT_REQ_LOCAL;
    	hdr.cmn.hdrLen = hdrLen;
    	hdr.cmn.totalLen = totalLen;
    	
    	hdr.src = srcAddr;
    	hdr.dst = dstAddr;
    	
    	SPH::setHeader(packet, hdr);
    	
    	// version number, try to get 0
    	*(uint32_t*)(packet+hdrLen) = 0;
    		
    	scionPrinter->printLog((char *)"Send ROT request (%u, %u) to Cert Server: %s\n", hdr.cmn.type, totalLen, m_servers.find(CertificateServer)->second.HID);
    		
    	// use explicit path instead of using destination (AD, HID) only
    	string dest = "RE ";
    	dest.append(" ");
    	dest.append(BHID);
		dest.append(" ");
    	// cert server 
    	dest.append("HID:");
    	dest.append((const char*)m_servers.find(CertificateServer)->second.HID);
    		
    	sendPacket(packet, totalLen, dest);
    	
    	_timer.reschedule_after_sec(1);     // default speed
  }
}

/*
    SCIONBeaconServerCore::sendPacket
    - Creates click packet and sends packet to the given port
    - Send packets via XIA engine
*/
void SCIONBeaconServerCore::sendPacket(uint8_t* data, uint16_t data_length, string dest) {

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

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONBeaconServerCore)

