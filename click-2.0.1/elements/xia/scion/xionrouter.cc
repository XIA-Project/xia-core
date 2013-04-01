

#include "xionrouter.hpp"

XionRouter::XionRouter(string conf_file){
    m_sConfigFile = string(conf_file);
//    initVariables(); 
    config.getOfgmKey(conf_file.c_str());
    
    initOfgKey();
    updateOfgKey();
}
XionRouter::~XionRouter(){

}

bool XionRouter::initOfgKey() {
  time_t currTime;
  time(&currTime);
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

bool XionRouter::updateOfgKey() {
	m_prevOfgKey.time = m_currOfgKey.time;
	memcpy(m_prevOfgKey.key, m_currOfgKey.key, OFG_KEY_SIZE);
	int err;
	if(err = aes_setkey_enc(&m_prevOfgKey.actx, m_prevOfgKey.key, OFG_KEY_SIZE_BITS)) {
		printf("Prev. Enc Key setup failure: %d\n",err*-1);
		return SCION_FAILURE;
	}
	return SCION_SUCCESS;
}

void XionRouter::initVariables(){
  Config config;
  config.parseConfigFile((char*)m_sConfigFile.c_str());
  config.getOfgmKey((char*)m_uMkey);
}

int XionRouter::handle_packet(uint8_t* inc_pkt, int port, uint8_t** out_pkt){
  uint16_t packetLength = SCIONPacketHeader::getTotalLen(inc_pkt);
  *out_pkt = (uint8_t*)malloc(packetLength);

  memset(*out_pkt, 0, packetLength);
  memcpy(*out_pkt, (uint8_t*)inc_pkt, packetLength);

  HostAddr src = SCIONPacketHeader::getSrcAddr(*out_pkt); 
  HostAddr dst = SCIONPacketHeader::getDstAddr(*out_pkt);


  uint8_t srcLen = SCIONPacketHeader::getSrcLen(*out_pkt);
  uint8_t dstLen = SCIONPacketHeader::getDstLen(*out_pkt);
  uint8_t pCurrOF = SCIONPacketHeader::getCurrOFPtr(*out_pkt);
  uint8_t *of = SCIONPacketHeader::getCurrOF(*out_pkt);
   
  //Local packet
  if(pCurrOF == (srcLen+dstLen)) {
    SCIONPacketHeader::increaseOFPtr(*out_pkt,1);
    of = SCIONPacketHeader::getCurrOF(*out_pkt);
    //int ret = forwardDataPacket(port, *out_pkt);
    return ((opaqueField*)(of))->ingressIf;
  }

  int out_port = forwardDataPacket(port, *out_pkt);
  if(*of == TDC_XOVR){ 
    SCIONPacketHeader::setTimestampPtr(*out_pkt,pCurrOF);
    SCIONPacketHeader::setDownpathFlag(*out_pkt);
    SCIONPacketHeader::increaseOFPtr(*out_pkt,1);
  } else if (*of == NON_TDC_XOVR){ 
    SCIONPacketHeader::setTimestampPtr(*out_pkt,pCurrOF);
    SCIONPacketHeader::setDownpathFlag(*out_pkt);
    SCIONPacketHeader::increaseOFPtr(*out_pkt,2);
  }

  return out_port;
}

/*
	SLN:
	normal forwarding to the next hop AD
*/
int XionRouter::normalForward(uint8_t type, int port, uint8_t * packet, uint8_t isUppath) {
  uint16_t packetLength = SCIONPacketHeader::getTotalLen(packet);
  if(verifyOF(port, packet)!=SCION_SUCCESS){
    return -1;
  }
  uint8_t * nof = SCIONPacketHeader::getCurrOF(packet);
  opaqueField * pOF = (opaqueField *) nof;
  uint16_t iface;

  if(isUppath)
    iface = pOF->ingressIf;
  else
    iface = pOF->egressIf;
  return iface;
}

int XionRouter::crossoverForward(uint8_t type, uint8_t info, int port, uint8_t * packet, uint8_t isUppath) {
  uint16_t packetLength = SCIONPacketHeader::getTotalLen(packet);
  uint8_t pCurrOF;
  uint16_t ifid;
  int isRegular = 1;
  int hops = 0;
  uint8_t nSpecialOF = 1; //by default, # of special OF at TDC is 1

  switch(info) {
    case TDC_XOVR:
      if(verifyOF(port, packet)!=SCION_SUCCESS){
        printf("MAC failed 1\n");
        return -1;
      }
      if(isUppath){
        SCIONPacketHeader::increaseOFPtr(packet, 1); //the next OF is the special OF
      }else{ //Downpath
        SCIONPacketHeader::increaseOFPtr(packet,1);
      }
      break;

    case NON_TDC_XOVR:
      if(verifyOF(port, packet) != SCION_SUCCESS){
        printf("MAC CHECK FAILED @ Crossover AD\n");
        return -1;
      }
      if(isUppath){
        SCIONPacketHeader::increaseOFPtr(packet, 2); //move pointer to the special OF
      } else{ //Downpath
        SCIONPacketHeader::increaseOFPtr(packet, 2);
      }
      break;

    case INPATH_XOVR:
      if(verifyOF(port, packet) != SCION_SUCCESS){
        printf("MAC CHECK FAILED @ On-Path\n");
        return -1;
      }
      while(isRegular) {
        pCurrOF = SCIONPacketHeader::increaseOFPtr(packet,1); 
        isRegular = SCIONPacketHeader::isRegularOF(packet, pCurrOF);
        hops++;
      }

      SCIONPacketHeader::setTimestampPtr(packet, pCurrOF);
      SCIONPacketHeader::increaseOFPtr(packet,hops); //symmetric on TS 
      if(verifyOF(port, packet) != SCION_SUCCESS){
        printf("MAC check 4\n");
        return -1;
      }
      break;

    case INTRATD_PEER:
      if(isUppath)
        SCIONPacketHeader::increaseOFPtr(packet, 1); 
      if(verifyOF(port, packet) != SCION_SUCCESS){
        printf("MAC CHECK FAILED @ Peering Link Up\n");
        return -1;
      }
      if(!isUppath)
        SCIONPacketHeader::increaseOFPtr(packet, 2); //move pointer to the special OF

      break;
    case INTERTD_PEER:
      break;

    default: 
      break;
  }
  uint8_t *of = SCIONPacketHeader::getCurrOF(packet);
  return ((opaqueField*)of)->egressIf;
}

int XionRouter::forwardDataPacket(int port, uint8_t * packet) {

  uint8_t pCurrOF = SCIONPacketHeader::getCurrOFPtr(packet); 	//CurrOF*   
  uint8_t *currOF = SCIONPacketHeader::getCurrOF(packet); 	//CurrOF   
  uint8_t oinfo = *currOF;

  uint8_t srcLen = SCIONPacketHeader::getSrcLen(packet);
  uint8_t dstLen = SCIONPacketHeader::getDstLen(packet);
  uint8_t type = SCIONPacketHeader::getType(packet);

  oinfo = oinfo >> 7;
  int isRegular = !oinfo; //check if MBS is 0

  //for the TDC AD edge router
  while(!isRegular) { //special opaque field; i.e., timestamp
    SCIONPacketHeader::setTimestampPtr(packet, pCurrOF);
    SCIONPacketHeader::setDownpathFlag(packet);
    pCurrOF = SCIONPacketHeader::increaseOFPtr(packet, 1);
    isRegular = SCIONPacketHeader::isRegularOF(packet, pCurrOF);
  } 

  uint8_t pTS = SCIONPacketHeader::getTimestampPtr(packet);	//TS*
  uint8_t info = SCIONPacketHeader::getTimestampInfo(packet, pTS); //Information field in the TS opaque field
  uint8_t of_type = SCIONPacketHeader::getOFType(packet);
  uint32_t TS = SCIONPacketHeader::getTimestamp(packet);	//Timestamp

  of_type = of_type << 1; 

  while(of_type & MASK_MSB){ 
    pCurrOF = SCIONPacketHeader::increaseOFPtr(packet,1);
    of_type = SCIONPacketHeader::getOFType(packet);
    of_type = of_type << 1; //1 bit shift left, 1st bit should be 0 since it's a regular OF
  }

  of_type = of_type << 1;

  uint8_t flag = SCIONPacketHeader::getFlags(packet);
  uint8_t isUppath = flag & MASK_MSB;

  if(!isUppath && (pCurrOF==pTS+OPAQUE_FIELD_SIZE) && (info&0x10)){
    pCurrOF = SCIONPacketHeader::increaseOFPtr(packet, 1);
  }

  if(!(of_type & MASK_MSB)) {
      return normalForward(type,port,packet,isUppath);
  } else { // cross over forwarding
      return crossoverForward(type,info,port,packet,isUppath);
  }
}




bool XionRouter::getOfgKey(uint32_t timestamp, aes_context &actx){
  if(timestamp > m_currOfgKey.time) {
    actx = m_currOfgKey.actx;
  } else {
    actx = m_prevOfgKey.actx;
  }
  return SCION_SUCCESS;

  std::map<uint32_t, aes_context*>::iterator itr;
  memset(&actx, 0, sizeof(aes_context));

  while(m_OfgAesCtx.size()>KEY_TABLE_SIZE){
    itr = m_OfgAesCtx.begin();
    delete itr->second;
    m_OfgAesCtx.erase(itr);
  }

  if((itr=m_OfgAesCtx.find(timestamp)) == m_OfgAesCtx.end()){

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

int XionRouter::verifyInterface(int port, uint8_t * packet, uint8_t dir) {
	uint16_t in_ifid, e_ifid, ifid;
	uint8_t type = SCIONPacketHeader::getType(packet);
  in_ifid = SCIONPacketHeader::getIngressInterface(packet);
  e_ifid = SCIONPacketHeader::getEgressInterface(packet);

  if((dir && e_ifid) || (!in_ifid && !dir)) 
		ifid = e_ifid;
	else
		ifid = in_ifid;
		
	int inPort = 0;
	
	if(port==ifid) {
		return SCION_SUCCESS;
	} else {
		return SCION_FAILURE;
	}
}

int XionRouter::verifyOF(int port, uint8_t * packet) {
  uint8_t flag = SCIONPacketHeader::getFlags(packet);
  uint8_t isUppath = flag & MASK_MSB;
  uint32_t timestamp = SCIONPacketHeader::getTimestamp(packet);

  if(verifyInterface(port, packet, isUppath) == SCION_FAILURE){
    printf("interface verification failed\n");
    return SCION_FAILURE;
  }

  aes_context actx;
  getOfgKey(timestamp,actx);
  uint8_t * nof = SCIONPacketHeader::getCurrOF(packet);
  uint8_t * ncof = SCIONPacketHeader::getOF(packet,1);
  opaqueField * of = (opaqueField *)nof;
  opaqueField * chained_of = (opaqueField *) ncof;
  opaqueField no = opaqueField();

  if(isUppath){
    if(of->ingressIf) {//Non-TDC
      ncof = SCIONPacketHeader::getOF(packet,1);
      chained_of = (opaqueField *) ncof;
    }else { //TDC
      chained_of = &no; //i.e., set to 0
    }
  }else{
    if(of->ingressIf){ //Non-TDC
      ncof = SCIONPacketHeader::getOF(packet,-1);
      chained_of = (opaqueField *) ncof;
    } else {//TDC
      chained_of = &no; //i.e., set to 0
    }
  }
  uint8_t exp = 0x03 & of->type;
  return SCIONBeaconLib::verifyMAC(timestamp, exp, of->ingressIf, of->egressIf,
      *(uint64_t *)&chained_of,of->mac,&actx);
}

