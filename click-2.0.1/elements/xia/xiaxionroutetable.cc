
#include <click/config.h>
#include "xiaxionroutetable.hh"

CLICK_DECLS

XIAXIONRouteTable::XIAXIONRouteTable() : run_mode(RUN_MODE_NOTREADY) { }

XIAXIONRouteTable::~XIAXIONRouteTable() { }

int XIAXIONRouteTable::configure(Vector<String> &conf, ErrorHandler *errh) {
  return 0;
}

void XIAXIONRouteTable::push(int in_ether_port, Packet *p) {
	in_ether_port = XIA_PAINT_ANNO(p);
//  click_chatter("%s: in port - %d", name().c_str(), in_ether_port);
//  SET_XIA_PAINT_ANNO(p, to what port?);
//  SET_XIA_PAINT_ANNO(p, 1);
//	in_ether_port = XIA_PAINT_ANNO(p);
//  click_chatter("%s: in port - %d", name().c_str(), in_ether_port);

  if (run_mode == RUN_MODE_NOTREADY) {
    click_chatter("%s: XION forwarding table not initialized", name().c_str());

  } else if (run_mode == RUN_MODE_AD) {
    uint8_t xion_path[1024];
    memset(xion_path, 0, 1024);

    int num_xid_block = extract_xion_path(p, xion_path);
    
    std::map<int, int>::iterator itr = port2ifid.find(in_ether_port);
    int in_ifid = itr->second;

    click_chatter("%s - inport: %d, inifid: %d", name().c_str(), in_ether_port, in_ifid);

    int out_ifid;
    uint8_t pCurrOF = SCIONPacketHeader::getCurrOFPtr(xion_path);
    if (pCurrOF  == SCIONPacketHeader::getSrcLen(xion_path) + SCIONPacketHeader::getDstLen(xion_path)) {
      out_ifid = forwardDataPacket(in_ifid, xion_path);
    } else {
      uint8_t *of = SCIONPacketHeader::getCurrOF(xion_path);
      if (*of == TDC_XOVR) {
					SCIONPacketHeader::setTimestampPtr(xion_path,pCurrOF);
					SCIONPacketHeader::setDownpathFlag(xion_path);
					SCIONPacketHeader::increaseOFPtr(xion_path,1);
      } else if (*of == NON_TDC_XOVR) {
					SCIONPacketHeader::setTimestampPtr(xion_path,pCurrOF);
					SCIONPacketHeader::setDownpathFlag(xion_path);
					SCIONPacketHeader::increaseOFPtr(xion_path,2);
      }
      SCIONPacketHeader::increaseOFPtr(xion_path, 1);
      out_ifid = forwardDataPacket(in_ifid, xion_path);
    }

/*
opaqueField *of = (opaqueField *)SCIONPacketHeader::getCurrOF(xion_path);
click_chatter("AAAAAAAAAAAAAAAAA - in: %d, out: %d", of->ingressIf, of->egressIf);
    int out_ifid = forwardDataPacket(in_ifid, xion_path);
of = (opaqueField *)SCIONPacketHeader::getCurrOF(xion_path);
click_chatter("BBBBBBBBBBBBBBBBB - in: %d, out: %d", of->ingressIf, of->egressIf);

SCIONPacketHeader::increaseOFPtr(xion_path, 1);
of = (opaqueField *)SCIONPacketHeader::getCurrOF(xion_path);
click_chatter("CCCCCCCCCCCCCCCCC - in: %d, out: %d", of->ingressIf, of->egressIf);
*/
    modify_xion_path(p, num_xid_block, xion_path);

    int out_ether_port;
    bool out_ether_port_found = false;

    for (itr=port2ifid.begin(); itr!=port2ifid.end(); itr++) {
      if (itr->second == out_ifid) {
        out_ether_port = itr->first;
        out_ether_port_found = true;
      }
    }

    if (!out_ether_port_found) {
      click_chatter("%s: not matching port for IFID %d", name().c_str(), out_ifid);
      return;
    }

    click_chatter("%s - outport: %d, outifid: %d", name().c_str(), out_ether_port, out_ifid);

    if (out_ifid == 0) {
      ((click_xia *)p->xia_header())->last += num_xid_block;
      
      output(1).push(p);
    } else {
      SET_XIA_PAINT_ANNO(p, out_ether_port);
      p->set_nexthop_neighbor_xid_anno(XID("HID:1111111111111111111111111111111111111111"));
      output(0).push(p);
    }

  } else if (run_mode == RUN_MODE_HOST) {
    SET_XIA_PAINT_ANNO(p, host_mode_default_forward_port);
    p->set_nexthop_neighbor_xid_anno(XID("HID:1111111111111111111111111111111111111111"));
    output(0).push(p);

  } else {
    click_chatter("%s: SHOULD NEVER HAPPEN!!", name().c_str());
  }
}

void XIAXIONRouteTable::add_handlers() {
  add_write_handler("initialize", initialize_handler, 0);
  add_write_handler("add_port2ifid", add_port2ifid_handler, 0);
}

int XIAXIONRouteTable::initialize_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh) {
  String conf_cpy = conf;
  XIAXIONRouteTable *xrt = (XIAXIONRouteTable *)e;

  String option = cp_shift_spacevec(conf_cpy);
  String argname = cp_shift_spacevec(conf_cpy);
  String argparam = cp_shift_spacevec(conf_cpy);

  if (option == "HOSTMODE") {
    if (argname == "DEFAULTPORT") {
      int default_forward_port;
      if (!cp_integer(argparam, &default_forward_port)) {
        return errh->error("invalid default forwarding port");
      } else {
        xrt->host_mode_default_forward_port = default_forward_port;
      }
      xrt->run_mode = RUN_MODE_HOST;
    } else {
      return errh->error("invalid HOSTMODE argument");
    }
  } else if (option == "ADMODE") {
    if (argname == "OFGKEY") {
      memset(xrt->ofg_master_key, 0, MASTER_KEY_LEN);
      memcpy(xrt->ofg_master_key, argparam.c_str(), argparam.length());
      xrt->init_ofg_key();
      xrt->update_ofg_key();

      xrt->run_mode = RUN_MODE_AD;
    } else {
      return errh->error("invalid ADMODE argument");
    }
  } else {
    return errh->error("invalid init mode");
  }
  return 0;
}

int XIAXIONRouteTable::add_port2ifid_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh) {
  XIAXIONRouteTable *xrt = (XIAXIONRouteTable *)e;

  String conf_cpy = conf;
  
  while (true) {
    String mapper_str = cp_shift_spacevec(conf_cpy);
    if (mapper_str.length() == 0)
      break;

    int delim = mapper_str.find_left(':');
    if (delim < 0)
      errh->error("invalid port2ifid argument, should be of format <port>:<ifid>");

    int port_num;
    int ifid_num;
    if (!cp_integer(mapper_str.substring(0, delim), &port_num))
      errh->error("invalid port number");
    if (!cp_integer(mapper_str.substring(delim+1, 100), &ifid_num))
      errh->error("invalid port number");

    xrt->port2ifid.insert(pair<int, int>(port_num, ifid_num));
  }
  return 0;
}

int XIAXIONRouteTable::extract_xion_path(Packet *p, uint8_t *xion_path) {
  XIAHeader xiah = p->xia_header();
  XIAPath dst_path = xiah.dst_path();
  uint8_t xion_node_front = xiah.last() + 1;
  uint8_t xion_node_offset = 0;

  while (true) {
    XID cur_node = dst_path.xid(xion_node_front + xion_node_offset);
    click_xia_xid xid = cur_node.xid();
    if (ntohl(xid.type) == CLICK_XIA_XID_TYPE_XION ||
        ntohl(xid.type) == CLICK_XIA_XID_TYPE_XION_) {
      memcpy(xion_path + (xion_node_offset * CLICK_XIA_XID_ID_LEN),
             xid.id, CLICK_XIA_XID_ID_LEN);
      xion_node_offset++;
    } else {
      break;
    }
  }

  return xion_node_offset;
}

void XIAXIONRouteTable::modify_xion_path(Packet *p, int num_xid_block, uint8_t *xion_path) {
  click_xia *xia_hdr = (click_xia *)p->xia_header();
  uint8_t xion_node_head = xia_hdr->last + 1;
  for (int i=0; i<num_xid_block; i++) {
    memcpy(xia_hdr->node[xion_node_head + i].xid.id,
           xion_path + CLICK_XIA_XID_ID_LEN * i,
           CLICK_XIA_XID_ID_LEN);
  }
}

bool XIAXIONRouteTable::init_ofg_key() {
  time_t curr_time;
  time(&curr_time);
  curr_time -= 300;
  curr_ofg_key.time = curr_time;
  memcpy(curr_ofg_key.key, ofg_master_key, OFG_KEY_SIZE);

  if (aes_setkey_enc(&curr_ofg_key.actx, curr_ofg_key.key, OFG_KEY_SIZE_BITS)) {
    click_chatter("xion route table key setup fail");
    return -1;
  }
  return 0;
}

bool XIAXIONRouteTable::update_ofg_key() {
  prev_ofg_key.time = curr_ofg_key.time;
  memcpy(prev_ofg_key.key, curr_ofg_key.key, OFG_KEY_SIZE);

  if (aes_setkey_enc(&prev_ofg_key.actx, prev_ofg_key.key, OFG_KEY_SIZE_BITS)) {
    click_chatter("xion route table key update fail");
    return -1;
  }
  return 0;
}

int XIAXIONRouteTable::forwardDataPacket(int port, uint8_t * packet) {
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
    if (pCurrOF == srcLen + dstLen) {
      SCIONPacketHeader::setUppathFlag(packet);
    } else {
      SCIONPacketHeader::setDownpathFlag(packet);
    }

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

int XIAXIONRouteTable::normalForward(uint8_t type, int port, uint8_t *packet, uint8_t isUppath) {
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

int XIAXIONRouteTable::crossoverForward(uint8_t type, uint8_t info, int port, uint8_t * packet, uint8_t isUppath) {
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
        SCIONPacketHeader::increaseOFPtr(packet, 2); //the next OF is the special OF
					SCIONPacketHeader::setDownpathFlag(packet);
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

int XIAXIONRouteTable::verifyOF(int port, uint8_t * packet) {
  uint8_t flag = SCIONPacketHeader::getFlags(packet);
  uint8_t isUppath = flag & MASK_MSB;
  uint32_t timestamp = SCIONPacketHeader::getTimestamp(packet);

  if (port) {
    if(verifyInterface(port, packet, isUppath) == SCION_FAILURE){
      printf("interface verification failed\n");
      return SCION_FAILURE;
    }
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

int XIAXIONRouteTable::verifyInterface(int port, uint8_t * packet, uint8_t dir) {
	uint16_t in_ifid, e_ifid, ifid;
	uint8_t type = SCIONPacketHeader::getType(packet);
  in_ifid = SCIONPacketHeader::getIngressInterface(packet);
  e_ifid = SCIONPacketHeader::getEgressInterface(packet);

  if((dir)) 
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

bool XIAXIONRouteTable::getOfgKey(uint32_t timestamp, aes_context &actx){
  if(timestamp > curr_ofg_key.time) {
    actx = curr_ofg_key.actx;
  } else {
    actx = prev_ofg_key.actx;
  }
  return SCION_SUCCESS;

  std::map<uint32_t, aes_context*>::iterator itr;
  memset(&actx, 0, sizeof(aes_context));

  while(ofg_aes_ctx.size()>KEY_TABLE_SIZE){
    itr = ofg_aes_ctx.begin();
    delete itr->second;
    ofg_aes_ctx.erase(itr);
  }

  if((itr=ofg_aes_ctx.find(timestamp)) == ofg_aes_ctx.end()){

    uint8_t k[SHA1_SIZE];
    memset(k, 0, SHA1_SIZE);
    memcpy(k, &timestamp, sizeof(uint32_t));
    memcpy(k+sizeof(uint32_t), ofg_master_key, OFG_KEY_SIZE);

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

    ofg_aes_ctx.insert(std::pair<uint32_t, aes_context*>(timestamp, pActx));
    actx = *ofg_aes_ctx.find(timestamp)->second;
  }else{
    actx = *ofg_aes_ctx.find(timestamp)->second;
  }
  return SCION_SUCCESS;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAXIONRouteTable)
