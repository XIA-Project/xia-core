#include <click/config.h>
#include "xiapingupdate.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAPingUpdate::XIAPingUpdate()
{
}

XIAPingUpdate::~XIAPingUpdate()
{
}

int
XIAPingUpdate::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
                   "DST", cpkP+cpkM, cpXIAPath, &_dst_path,
                   "NEW", cpkP+cpkM, cpXIAPath, &_new_path,
                   cpEnd) < 0)
        return -1;

    return 0;
}

int
XIAPingUpdate::initialize(ErrorHandler *)
{
    return 0;
}

Packet *
XIAPingUpdate::pull(int)
{
    return make_packet(_src_path, _dst_path, _new_path);
}

WritablePacket *
XIAPingUpdate::make_packet(const XIAPath& src_path, const XIAPath& dst_path, const XIAPath& new_path)
{
    size_t byte_size = sizeof(struct click_xia_xid_node) * new_path.unparse_node_size();
    WritablePacket *p = Packet::make(256, NULL, byte_size, 0);

    new_path.unparse_node(reinterpret_cast<struct click_xia_xid_node*>(p->data()), new_path.unparse_node_size());
    
    XIAHeaderEncap encap;
    encap.set_nxt(102);   // UPDATE
    encap.set_dst_path(dst_path);
    encap.set_src_path(src_path);
    return encap.encap(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAPingUpdate)
ELEMENT_MT_SAFE(XIAPingUpdate)
