#include <click/config.h>
#include "xiadhcpserver.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XIADHCPServer::XIADHCPServer()
{
}

XIADHCPServer::~XIADHCPServer()
{
}

int
XIADHCPServer::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                   "SRC_PATH", cpkP+cpkM, cpXIAPath, &_src_path,
                   "BROADCAST_PATH", cpkP+cpkM, cpXIAPath, &_broadcast_path,
                   "AD_PATH", cpkP+cpkM, cpXIAPath, &_ad_path,
                   cpEnd) < 0)
        return -1;

    return 0;
}

int
XIADHCPServer::initialize(ErrorHandler *)
{
    return 0;
}

Packet *
XIADHCPServer::pull(int) {
    WritablePacket *p = WritablePacket::make(256, _ad_path.unparse().c_str(), strlen(_ad_path.unparse().c_str())+1, 0);
//    WritablePacket *p = WritablePacket::make(256, NULL, 0, 0);
    p->data()[strlen(_ad_path.unparse().c_str())] = '\0';
    XIAHeaderEncap encap;
    encap.set_nxt(99);
    encap.set_dst_path(_broadcast_path);
    encap.set_src_path(_src_path);
    return encap.encap(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIADHCPServer)
