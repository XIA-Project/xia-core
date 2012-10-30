/*
 * xiancap.{cc,hh} -- element encapsulating packets in an XIA header
 */

#include <click/config.h>
#include "xiaencap.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAEncap::XIAEncap()
    : _xiah(NULL), _contenth(NULL), _is_dynamic(false)
{
    _xiah = new XIAHeaderEncap();
}

XIAEncap::~XIAEncap()
{
    delete _xiah;
    delete _contenth;
}

int
XIAEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    XIAPath src_path;
    XIAPath dst_path;
    int nxt = -1;
    int last = -1;
    uint8_t hlim = 250;
    int packet_offset = -1;
	int chunk_offset = -1;
	int content_length = -1;
	int chunk_length = -1;
    bool is_request =false;
    bool is_dynamic =false;

    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &src_path,
                   "DST", cpkP+cpkM, cpXIAPath, &dst_path,
                   "NXT", 0, cpInteger, &nxt,
                   "LAST", 0, cpInteger, &last,
                   "HLIM", 0, cpByte, &hlim,
                   "DYNAMIC", 0, cpBool, &is_dynamic,
                   "EXT_C_REQUEST", 0, cpBool, &is_request,
                   "EXT_C_PACKET_OFFSET", 0, cpInteger, &packet_offset,
                   "EXT_C_CHUNK_OFFSET", 0, cpInteger, &chunk_offset,
                   "EXT_C_CONTENT_LENGTH", 0, cpInteger, &content_length,
                   "EXT_C_CHUNK_LENGTH", 0, cpInteger, &chunk_length,
                   cpEnd) < 0)
        return -1;

    //click_chatter("%s", src_path.unparse().c_str());
    //click_chatter("%s", dst_path.unparse().c_str());

    if (nxt < -1 || nxt > 255)
        return errh->error("bad next protocol");

    if (nxt >= 0)
        _xiah->set_nxt(nxt);
    _xiah->set_last(last);
    _xiah->set_hlim(hlim);
    _xiah->set_dst_path(dst_path);
    _xiah->set_src_path(src_path);

    if (chunk_length!=-1) {
        _contenth  = new ContentHeaderEncap(packet_offset, chunk_offset, content_length, chunk_length);
        if (nxt >= 0)
            _contenth->set_nxt(nxt);
        _contenth->update();
        _xiah->set_nxt(CLICK_XIA_NXT_CID);
        click_chatter("EXT RESPONSE %d %d %d %d\n", packet_offset, chunk_offset, content_length, chunk_length );
    } else if (is_request) {
        _contenth  = ContentHeaderEncap::MakeRequestHeader();
        if (nxt >= 0)
            _contenth->set_nxt(nxt);
        _contenth->update();
        _xiah->set_nxt(CLICK_XIA_NXT_CID);
        click_chatter("EXT REQUEST %d %d %d %d\n", packet_offset, chunk_offset, content_length, chunk_length );
    }
    _is_dynamic = is_dynamic;

    return 0;
}

int
XIAEncap::initialize(ErrorHandler *)
{
    return 0;
}

Packet *
XIAEncap::simple_action(Packet *p_in)
{
    WritablePacket *p = NULL;
    size_t length = p_in->length();
    if (_contenth) {
        p = _contenth->encap(p_in);
        if (p) {
			// set payload length ignoring the content ext header
            _xiah->set_plen(length);
            p = _xiah->encap(p, false);
        }
    }
    else {
		if (_is_dynamic) {
			if (_xiah->dst_path().unparse_node_size() > 1)
				_xiah->dst_path().incr(2);
			else if (_xiah->dst_path().unparse_node_size() == 1)
				_xiah->src_path().incr(1);
			_xiah->update();
		}
		p = _xiah->encap(p_in, true);
    }
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAEncap)
ELEMENT_MT_SAFE(XIAEncap)
