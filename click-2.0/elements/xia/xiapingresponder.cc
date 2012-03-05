#include <click/config.h>
#include "xiapingresponder.hh"
#include "xiapingupdate.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAPingResponder::XIAPingResponder()
{
    _count = 0;
    _connected = false;
}

XIAPingResponder::~XIAPingResponder()
{
}

int
XIAPingResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
                   cpEnd) < 0)
        return -1;

    return 0;
}

int
XIAPingResponder::initialize(ErrorHandler *)
{
    return 0;
}

void
XIAPingResponder::push(int, Packet *p_in)
{
    XIAHeader hdr(p_in);

    switch (hdr.nxt()) {
		case 100:
			{
				// PING
				if (hdr.plen() != 8) {
					click_chatter("invalid PING message length\n");
					break;
				}

				click_chatter("%u: PING received; client seq = %u\n", p_in->timestamp_anno().usecval(), *(uint32_t*)(hdr.payload() + 0));

				WritablePacket *p = Packet::make(256, NULL, 8, 0);
				
				*(uint32_t*)(p->data() + 0) = *(uint32_t*)(hdr.payload() + 0);
				*(uint32_t*)(p->data() + 4) = _count++;

				XIAHeaderEncap encap;
				encap.set_nxt(101);   // PONG
				encap.set_dst_path(hdr.src_path());
				encap.set_src_path(_src_path);

				_connected = true;
				_last_client = hdr.src_path();

				click_chatter("%u: PONG sent; client seq = %u, server seq = %u\n", Timestamp::now().usecval(), *(uint32_t*)(p->data() + 0), *(uint32_t*)(p->data() + 4));

				output(0).push(encap.encap(p));
				break;
			}

		case 101:
			// PONG
			click_chatter("ignoring PONG at XIAPingResponder\n");
			break;

		case 102:
			// UPDATE
			{
				XIAPath new_path;
				new_path.parse_node(
					reinterpret_cast<const struct click_xia_xid_node*>(hdr.payload()),
					reinterpret_cast<const struct click_xia_xid_node*>(hdr.payload() + hdr.plen())
				);
				click_chatter("%u: updating XIAPingResponder with new address %s\n", p_in->timestamp_anno().usecval(), new_path.unparse(this).c_str());
				_src_path = new_path;

				if (_connected)
					output(0).push(XIAPingUpdate::make_packet(_src_path, _last_client, _src_path));


				break;
			}

		default:
			click_chatter("invalid message type\n");
			break;
    }

    p_in->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAPingResponder)
ELEMENT_MT_SAFE(XIAPingResponder)
