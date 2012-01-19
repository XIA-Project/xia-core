#include <click/config.h>
#include "xcmp.hh"
//#include "xiapingupdate.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XCMP::XCMP()
{
  //_count = 0;
  //_connected = false;
}

XCMP::~XCMP()
{
}

int
XCMP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
                   cpEnd) < 0)
        return -1;

    return 0;
}

int
XCMP::initialize(ErrorHandler *)
{
    return 0;
}

void
XCMP::push(int, Packet *p_in)
{
    XIAHeader hdr(p_in);
    const uint8_t *payload = hdr.payload();

    if(hdr.nxt() != 15) { // not XCMP
      p_in->kill();
      return;
    }


    switch (*payload) {
    case 8: // PING
        {
	  /*if (hdr.plen() != 8) {
                click_chatter("invalid PING message length\n");
                break;
		}*/

            click_chatter("%s: %u: PING received; client seq = %u\n", _src_path.unparse().c_str(),p_in->timestamp_anno().usecval(), *(uint16_t*)(payload + 6));

            WritablePacket *p = Packet::make(256, NULL, 8, 0);
            
            *(uint32_t*)(p->data() + 0) = *(uint32_t*)(hdr.payload() + 0);
            *(uint32_t*)(p->data() + 4) = *(uint32_t*)(hdr.payload() + 4);

	    *(uint8_t*)(p->data() + 0) = 0; // PONG

            XIAHeaderEncap encap;
            encap.set_nxt(15);   // XCMP
            encap.set_dst_path(hdr.src_path());
            encap.set_src_path(_src_path);

            //_connected = true;
            //_last_client = hdr.src_path();

            click_chatter("%s: %u: PONG sent; client seq = %u\n", _src_path.unparse().c_str(),Timestamp::now().usecval(), *(uint16_t*)(p->data() + 6));

            output(0).push(encap.encap(p));
            break;
        }

    case 0: // PONG
      click_chatter("%s: %u: PONG recieved; client seq = %u\n", _src_path.unparse().c_str(), Timestamp::now().usecval(), *(uint16_t*)(payload + 6));
      //click_chatter("ignoring PONG at XCMP\n");
        break;

	/*    case 102:
        // UPDATE
        {
            XIAPath new_path;
            new_path.parse_node(
                reinterpret_cast<const struct click_xia_xid_node*>(hdr.payload()),
                reinterpret_cast<const struct click_xia_xid_node*>(hdr.payload() + hdr.plen())
            );
            click_chatter("%u: updating XCMP with new address %s\n", p_in->timestamp_anno().usecval(), new_path.unparse(this).c_str());
            _src_path = new_path;

            if (_connected)
                output(0).push(XIAPingUpdate::make_packet(_src_path, _last_client, _src_path));


            break;
	    }*/

    default:
        click_chatter("invalid message type\n");
        break;
    }

    p_in->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XCMP)
ELEMENT_MT_SAFE(XCMP)
