#include "xion_bridge.hh"
#include "markxiaheader.hh"
#include <click/packet_anno.hh>

CLICK_DECLS

XIONBridge::XIONBridge() : _timer(this) {
}

int XIONBridge::configure(Vector<String> &conf, ErrorHandler *errh) {
  if (Args(conf, this, errh)
      .read_m("AID", scion_aid)
      .read_m("ADID", scion_adid)
      .read_m("TOPOLOGY_FILE", StringArg(), topology_file)
      .complete() < 0) {
    return -1;
  }
  return 0;
}

int XIONBridge::initialize(ErrorHandler *errh) {
  initVariables();
  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}

void XIONBridge::initVariables() {
  path_table = new SCIONPathInfo;
  TopoParser parser;
  parser.loadTopoFile(topology_file.c_str());
  parser.parseServers(servers);
  parser.parseRouters(routers);
}

void XIONBridge::push(int port, Packet *p) {
  const uint8_t *data = p->data();
  uint16_t pkt_len = SCIONPacketHeader::getTotalLen((uint8_t *)data);
  uint8_t pkt[pkt_len];

  memcpy(pkt, data, pkt_len);
  
  int pkt_type = SCIONPacketHeader::getType(pkt);

  if (port == 0) {
    if (pkt_type == UP_PATH) {
      path_table->parse(pkt, 0);
    } else if (pkt_type == PATH_REP_LOCAL) {
      path_table->parse(pkt, 1);
    } else if (pkt_type == AID_REQ) {
      
      HostAddr srcAddr = HostAddr(HOST_ADDR_SCION, scion_aid);
      SCIONPacketHeader::setType(pkt, AID_REP);
      SCIONPacketHeader::setSrcAddr(pkt, srcAddr);
      HostAddr dstAddr = SCIONPacketHeader::getDstAddr(pkt);

      if (dstAddr == servers.find(BeaconServer)->second.addr
          || dstAddr == servers.find(PathServer)->second.addr) {
        dstAddr = HostAddr(HOST_ADDR_SCION, scion_aid);
      }

      WritablePacket *out = Packet::make(DEFAULT_HD_ROOM, pkt, pkt_len, DEFAULT_TL_ROOM);
      output(0).push(out);
    } else {
      click_chatter("[%s] unexpected packet from SCION side", name().c_str());
      int hdrlen = SCIONPacketHeader::getHdrLen(pkt);
      for (int i=0; i<200+hdrlen; i++) { fprintf(stderr, "%02x", p->data()[i]);} fprintf(stderr, "\n");
      p->pull(hdrlen);
      for (int i=0; i<200; i++) { fprintf(stderr, "%02x", p->data()[i]);} fprintf(stderr, "\n");

      output(1).push(p);
    }
  } else {
    // unparse xid to get the target AD number
    XIAHeader xiah = XIAHeader(p);
    String xid_str = xiah.dst_path().xid(xiah.last() + 1).unparse();
    int delim_pos = xid_str.find_left(':');
    String ad_num_str = xid_str.substring(delim_pos + 1, CLICK_XIA_XID_ID_LEN * 2);
    unsigned long int ad_number = strtol(ad_num_str.c_str(), NULL, 16);
    click_chatter("%d", ad_number);
    pkt_cache.push_back(std::make_pair(ad_number, p));
  }
}

void XIONBridge::add_handlers() {
  set_handler("request_scion_path",
              Handler::OP_READ | Handler::READ_PARAM,
              request_scion_path_handler,
              0,
              0);
}

int XIONBridge::request_scion_path_handler(int flags, String &str, Element *e, const Handler *h, ErrorHandler *errh) {
  Vector<String> args;
  cp_argvec(str, args);
  uint64_t target;
  int rtn_flag = 0;
  XIONBridge *xb = (XIONBridge *)e;

  if (args.size() != 1) {
    return errh->error("invalid request");
  }
  if (!cp_integer(args[0], &target)) {
    return errh->error("invalid target");
  }

  click_chatter("request got for target: %d", target);

  fullPath path;
  path.opaque_field = NULL;
  if (xb->path_table->get_path(xb->scion_adid, target, path)) {
    rtn_flag = HANDLER_RTN_FLAG_FULLPATH;
    str = "found\n";
  } else {
    rtn_flag = HANDLER_RTN_FLAG_NOTREADY;
    xb->send_path_request(target);
    str = "fail\n";
  }
  return 0;
}

void XIONBridge::run_timer(Timer *timer) {
  // check each sec for cached packet to route to target ad
  if (!pkt_cache.empty()) {
    std::vector< std::pair<int, Packet *> >::iterator it = pkt_cache.begin();
    while (true) {
      if (it == pkt_cache.end()) {
        break;
      }
      fullPath path;
      path.opaque_field = NULL;
      if (path_table->get_path(scion_adid, it->first, path)) {
        //// actual encap sending
        scionHeader hdr;
        hdr.src = HostAddr(HOST_ADDR_SCION, (uint64_t)44444);
        hdr.dst = HostAddr(HOST_ADDR_SCION, (uint64_t)44444);
        uint8_t srcLen = hdr.src.getLength();
        uint8_t dstLen = hdr.dst.getLength();
        uint8_t offset = srcLen + dstLen;
        uint8_t hdrLen = path.length + COMMON_HEADER_SIZE + offset;
        hdr.cmn.type = DATA;
        hdr.cmn.hdrLen = hdrLen;
        hdr.cmn.totalLen = hdrLen + it->second->length();
        hdr.cmn.timestamp = offset;
        hdr.cmn.flag |= 0x80;
        hdr.cmn.currOF = offset;
        hdr.n_of = path.length / OPAQUE_FIELD_SIZE;
        hdr.p_of = path.opaque_field;
        XIAHeader xiah = it->second->xia_header();
        String xid_str = xiah.dst_path().xid(xiah.last() + 1).unparse();
        click_chatter("%s", xid_str.c_str());

        for (int i=0; i<200; i++) { fprintf(stderr, "%02x", it->second->data()[i]);} fprintf(stderr, "\n");
        WritablePacket *newp = it->second->push(hdrLen);
        uint8_t *header = (uint8_t *)newp->data();
        SCIONPacketHeader::setHeader(header, hdr);
        for (int i=0; i<200 + hdrLen; i++) { fprintf(stderr, "%02x", it->second->data()[i]);} fprintf(stderr, "\n");
        output(0).push(newp);
        pkt_cache.erase(it);
      } else {
        send_path_request(it->first);
        it++;
      }
    }
  }
  _timer.reschedule_after_sec(1);
}

void XIONBridge::send_path_request(uint64_t target) {
  HostAddr src = HostAddr(HOST_ADDR_SCION, scion_aid);
  uint16_t pkt_length = COMMON_HEADER_SIZE + SCION_ADDR_SIZE + src.getLength() + PATH_INFO_SIZE;
  uint8_t pkt[pkt_length];
  memset(pkt, 0, pkt_length);
  
  scionHeader hdr;
  HostAddr ps = servers.find(PathServer)->second.addr;
  hdr.src = src;
  hdr.dst = ps;
  hdr.cmn.type = PATH_REQ_LOCAL;
  hdr.cmn.hdrLen = COMMON_HEADER_SIZE + hdr.src.getLength() + hdr.dst.getLength();
  hdr.cmn.totalLen = pkt_length;
  SCIONPacketHeader::setHeader(pkt, hdr);
  
  pathInfo *info = (pathInfo *)(pkt + COMMON_HEADER_SIZE + src.getLength() + SCION_ADDR_SIZE);
  info->target = target;
  info->tdid = 1; // TODO: no hard coding
  info->option = 0;
  
  WritablePacket *out_packet = Packet::make(DEFAULT_HD_ROOM, pkt, pkt_length, DEFAULT_TL_ROOM);
  output(0).push(out_packet);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIONBridge)
