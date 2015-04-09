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
            click_chatter("Fatal ERR: click configuration fail at SCIONBeaconServerCore.\n");
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
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s) Initializes.\n", 
    m_AD.c_str(), m_HID.c_str());
    #endif

    // task 2: parse ROT (root of trust) file
    m_bROTInitiated = parseROT();
    if(m_bROTInitiated) {
        #ifdef _DEBUG_BS
        scionPrinter->printLog(IH, (char *)"Parse/Verify ROT Done.\n");
        #endif
    }

    // task 3: parse topology file
    parseTopology();
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"Parse Topology Done.\n");
    #endif

    // task 4: read key pair if they already exist
    loadPrivateKey();
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"Load Private Key Done.\n");
    #endif

    // for OFG fields
    if(!initOfgKey()){
        click_chatter("Fatal ERR: Init OFG Key fail.\n");
        exit(-1);
    }
    if(!updateOfgKey()){
        click_chatter("Fatal ERR: update OFG Key fail.\n");
        exit(-1);
    }
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"Load OFG key Done.\n");
    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s) Initialization Done.\n", 
        m_AD.c_str(), m_HID.c_str());
    #endif

    // Trigger Timer
    _timer.initialize(this);
    //start after other elements are initialized
    _timer.schedule_after_sec(10);
    
    return 0;
}

bool SCIONBeaconServerCore::parseROT(){
    ROTParser parser;
    if(parser.loadROTFile(m_sROTFile)!=ROTParseNoError){
        #ifdef _DEBUG_BS
        scionPrinter->printLog(EH, (char *)"ROT missing at TDC BS.\n");
        #endif
        return SCION_FAILURE;
    }else{
        // ROT found in local folder
        if(parser.parse(m_cROT)!=ROTParseNoError){
            #ifdef _DEBUG_BS
            scionPrinter->printLog(EH, (char *)"ROT parse error at TDC BS.\n");
            #endif
            return SCION_FAILURE;
        }

        if(parser.verifyROT(m_cROT)!=ROTParseNoError){
            #ifdef _DEBUG_BS
            scionPrinter->printLog(EH, (char *)"ROT verify error at TDC BS.\n");
             #endif
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
            click_chatter("Fatal ERR: Private key file loading failure at %s", 
            m_csPrvKeyFile);
            exit(-1);
        }
        if(rsa_check_privkey(&PriKey)!=0) {
            rsa_free(&PriKey);
            click_chatter("Fatal ERR: Private key %s has wrong format.", 
            m_csPrvKeyFile);
            exit(-1);
        }
        _CryptoIsReady = true;
    }
}

bool SCIONBeaconServerCore::initOfgKey() {
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

bool SCIONBeaconServerCore::updateOfgKey() {
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

bool SCIONBeaconServerCore::getOfgKey(uint32_t timestamp, aes_context &actx)
{
    if(timestamp > m_currOfgKey.time) {
        actx = m_currOfgKey.actx;
    } else {
        actx = m_prevOfgKey.actx;
    }
    return SCION_SUCCESS;
}

void SCIONBeaconServerCore::push(int port, Packet *p)
{
    TransportHeader thdr(p);
    uint8_t *s_pkt = (uint8_t *) thdr.payload();
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
            SPH::getHdrLen(packet);
            ROTRequest *req = (ROTRequest *)SPH::getData(packet);
            int curver = (int)req->currentVersion;
            #ifdef _DEBUG_BS
            scionPrinter->printLog(IH, SPH::getType(packet), (char *)"ROT Replay cV = %d, pV = %lu\n", 
                curver, m_cROT.version);
            #endif
            
            if(curver >= m_cROT.version) {
            
                uint16_t offset = SPH::getHdrLen(packet)+sizeof(ROTRequest);
                uint16_t rotLen = packetLength-offset;
                // Write to file defined in Config
                #ifdef _DEBUG_BS
                scionPrinter->printLog(IH, type, 
                (char *)"TDC BS (%s:%s): Received ROT file (size = %d).\n", 
                m_AD.c_str(), m_HID.c_str(), rotLen);
                #endif
                // Write to file defined in Config
                if(rotLen)
                {
                    // save ROT file to storage
                    FILE* rot = fopen(m_sROTFile, "w+");
                    fwrite(packet+offset, 1, rotLen, rot);
                    fclose(rot);
                    // try to verify local ROT again
                    m_bROTInitiated = parseROT();
                    if(m_bROTInitiated) {
                        #ifdef _DEBUG_BS
                        scionPrinter->printLog(IH, 
                        (char *)"TDC BS (%s:%s): stored verified ROT.\n", 
                        m_AD.c_str(), m_HID.c_str());
                        #endif
                    }
                }
            }else{
                // downgrade, should be error or attacks!
    	    }
        }
    		break;
        default:
        	break;
    }
}

bool SCIONBeaconServerCore::generateNewPCB() {

    // Create PCB for propagation
    uint16_t sigLen = PriKey.len;
    // Get current time for timestamp
    timeval tv;
    gettimeofday(&tv, NULL); //use time for lower precision

    // Create beacon
    uint8_t hdrLen = COMMON_HEADER_SIZE+AIP_SIZE*2;
    uint16_t totalLen =hdrLen+OPAQUE_FIELD_SIZE*2+PCB_MARKING_SIZE+sigLen;
    uint8_t buf[totalLen];
    memset(buf, 0, totalLen); //empty scion header
     
    scionHeader hdr; 
    hdr.cmn.type = BEACON;
    hdr.cmn.hdrLen = hdrLen;
    // Two opaque fields are used by TDC AD
    hdr.cmn.totalLen = hdrLen+OPAQUE_FIELD_SIZE*2;
    hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)strchr(m_HID.c_str(),':'));
    hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)"");
    SPH::setHeader(buf, hdr);
     
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s): Init Beacon with ROT Version: %d\n", 
        m_AD.c_str(), m_HID.c_str(), m_cROT.version);
    #endif
    SCIONBeaconLib::initBeaconInfo(buf,tv.tv_sec,m_uTdAid,m_cROT.version);
     
    // TimeStamp OFG field
    specialOpaqueField sOF = {SPECIAL_OF,tv.tv_sec,m_uTdAid,0};
    SPH::setTimestampOF(buf, sOF);
     
    // Get the OFG key that corresponds to the current timestamp 
    // and create aes context (i.e., key schedule)
    aes_context actx;
    if(!getOfgKey(tv.tv_sec, actx)) {
        //OFG key retrieval failure.
        #ifdef _DEBUG_BS
        scionPrinter->printLog(EH, (char *)"TDC BS (%s:%s): fail to get ofg key. Stop PCB generation.\n", 
            m_AD.c_str(), m_HID.c_str());
        #endif
        return SCION_FAILURE;
    }
    
    // iterators for all egress-ingress pairs
    std::multimap<int, EgressIngressPair>::iterator cItr;
    std::pair<std::multimap<int, EgressIngressPair>::iterator,
    std::multimap<int,EgressIngressPair>::iterator> childRange;
    childRange = m_routepairs.equal_range(Child);

    int cCount = 0;
    
    // OFG fields iterating for all child domains
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
        
        SPH::setDstAddr(msg, HostAddr(HOST_ADDR_AIP, (uint8_t*)rpair.egress_addr)); 
        // xia does not have interface id yet..
        SCIONBeaconLib::setInterface(msg, (uint16_t)strtoull((const char*)rpair.ingress_addr, NULL, 10));
        //sign the above marking
        uint64_t next_aid = strtoull((const char*)rpair.ingress_ad, NULL, 10);
        // end of patch
        
        SCIONBeaconLib::signPacket(msg, sigLen, next_aid, &PriKey);
        uint16_t msgLength = SPH::getTotalLen(msg);

        #ifdef _DEBUG_BS
        scionPrinter->printLog(IH,BEACON,tv.tv_sec,1,(uint64_t)strtoull((const char*)rpair.ingress_ad, NULL, 10),(char *)"%u,SENT\n",msgLength);
        #endif

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
  
    #ifdef _DEBUG_BS
    scionPrinter->printLog(IH, (char *)"TDC BS (%s:%s) delivers beacons to %d domains.\n",
        m_AD.c_str(), m_HID.c_str(), cCount);
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

void SCIONBeaconServerCore::run_timer(Timer *){

    sendHello();
    if(m_bROTInitiated&&_CryptoIsReady){
    
        // ROT file is ready and private key is ready
        generateNewPCB();
    	_timer.reschedule_after_sec(m_iPCBGenPeriod);

    }else if(!m_bROTInitiated){

    	// Request ROT from Cert Server
    	if(m_servers.find(CertificateServer)!=m_servers.end()){
    	    uint8_t hdrLen = COMMON_HEADER_SIZE+AIP_SIZE*2;
    	    uint16_t totalLen = hdrLen + sizeof(struct ROTRequest);
    	    uint8_t packet[totalLen];

    	    scionHeader hdr;
    	    hdr.cmn.type = ROT_REQ_LOCAL;
    	    hdr.cmn.hdrLen = hdrLen;
    	    hdr.cmn.totalLen = totalLen;
    	    hdr.src = HostAddr(HOST_ADDR_AIP, (uint8_t*)strchr(m_HID.c_str(),':'));
    	    hdr.dst = HostAddr(HOST_ADDR_AIP, (uint8_t*)(m_servers.find(CertificateServer)->second.HID));
    	    SPH::setHeader(packet, hdr);
    	    
    	    // set default version number as 0
    	    struct ROTRequest req;
    	    req.previousVersion = 0;
    	    req.currentVersion = 0; 
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
    	    scionPrinter->printLog(EH, (char *)"TDC BS (%s:%s): ROT is missing or wrong formatted. \
Send ROT request to local cert server.\n", m_AD.c_str(), m_HID.c_str());
            #endif
    	}else{
    	   #ifdef _DEBUG_BS
    	   scionPrinter->printLog(EH, (char*)"AD (%s) does not has cert server.\n", m_AD.c_str());
    	   #endif 
    	}

    	_timer.reschedule_after_sec(5);     // default speed
  }
}

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
    // XIA payload = transport header + transport-layer data
    xiah.set_plen(data_length + thdr->hlen());

    q = xiah.encap(q, false);
	output(0).push(q);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONBeaconServerCore)

