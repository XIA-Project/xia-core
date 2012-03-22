/*
 * xiaprint.{cc,hh} -- element prints packet contents to system log
 */

#include <click/config.h>
#include "xiaprint.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>
#include <click/xiaheader.hh>
#include <click/xiacontentheader.hh>

#if CLICK_USERLEVEL
# include <stdio.h>
#endif

CLICK_DECLS

XIAPrint::XIAPrint()
{
#if CLICK_USERLEVEL
    _outfile = 0;
#endif
}

XIAPrint::~XIAPrint()
{
}

int
XIAPrint::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _bytes = 1500;
    String contents = "no";
    String payload = "no";
    _label = "";
    _payload = false;
    _active = true;
    bool print_time = true;
    bool print_paint = false;
    bool print_hlim = false;
    bool print_len = false;
    bool print_aggregate = false;
    bool bcontents;
    String channel;
  
    if (cp_va_kparse(conf, this, errh,
		   "LABEL", cpkP, cpString, &_label,
		   "CONTENTS", 0, cpWord, &contents,
		   "PAYLOAD", 0, cpWord, &payload,
		   "MAXLENGTH", 0, cpInteger, &_bytes,
		   "NBYTES", 0, cpInteger, &_bytes, // deprecated
		   "TIMESTAMP", 0, cpBool, &print_time,
		   "PAINT", 0, cpBool, &print_paint,
		   "HLIM", 0, cpBool, &print_hlim,
		   "LENGTH", 0, cpBool, &print_len,
		   "AGGREGATE", 0, cpBool, &print_aggregate,
		   "ACTIVE", 0, cpBool, &_active,
#if CLICK_USERLEVEL
		   "OUTFILE", 0, cpFilename, &_outfilename,
#endif
		   "CHANNEL", 0, cpWord, &channel,
		   cpEnd) < 0)
		return -1;

	if (cp_bool(contents, &bcontents))
		_contents = bcontents;
	else if ((contents = contents.upper()), contents == "NONE")
		_contents = 0;
	else if (contents == "HEX")
		_contents = 1;
	else if (contents == "ASCII")
		_contents = 2;
	else
		return errh->error("bad contents value '%s'; should be 'NONE', 'HEX', or 'ASCII'", contents.c_str());

    int payloadv;
    payload = payload.upper();
    if (payload == "NO" || payload == "FALSE")
    	payloadv = 0;
    else if (payload == "YES" || payload == "TRUE" || payload == "HEX")
    	payloadv = 1;
    else if (payload == "ASCII")
    	payloadv = 2;
    else
    	return errh->error("bad payload value '%s'; should be 'false', 'hex', or 'ascii'", contents.c_str());
  
    if (payloadv > 0 && _contents > 0)
    	return errh->error("specify at most one of PAYLOAD and CONTENTS");
    else if (payloadv > 0)
    	_contents = payloadv, _payload = true;
  
    _print_timestamp = print_time;
    _print_paint = print_paint;
    _print_hlim = print_hlim;
    _print_len = print_len;
    _print_aggregate = print_aggregate;
    _errh = router()->chatter_channel(channel);
    return 0;
}

int
XIAPrint::initialize(ErrorHandler *errh)
{
#if CLICK_USERLEVEL
    if (_outfilename) {
    	_outfile = fopen(_outfilename.c_str(), "wb");
    	if (!_outfile)
        	return errh->error("%s: %s", _outfilename.c_str(), strerror(errno));
    }
#else
    (void) errh;
#endif
    return 0;
}

void
XIAPrint::cleanup(CleanupStage)
{
#if CLICK_USERLEVEL
    if (_outfile)
		fclose(_outfile);
    _outfile = 0;
#endif
}

void XIAPrint::print_xids(StringAccum &sa, const struct click_xia *xiah)
{
    XIAHeader h(xiah);
    XIAPath src, dst;

    src = h.src_path();
    dst = h.dst_path();

    String s;

    sa << "SRC " << src.unparse(this);
    sa << ", DST " << dst.unparse(this);
}

Packet *
XIAPrint::simple_action(Packet *p)
{
    if (!_active || !p->has_network_header())
		return p;

    StringAccum sa;

    if (_label)
		sa << _label << ": ";
    if (_print_timestamp)
		sa << p->timestamp_anno() << ": ";
    if (_print_aggregate)
		sa << '#' << AGGREGATE_ANNO(p);
    if (_print_paint)
		sa << (_print_aggregate ? "." : "paint ") << (int)PAINT_ANNO(p);
    if (_print_aggregate || _print_paint)
		sa << ": ";

    const click_xia *xiah = p->xia_header();
    int hdr_len = XIAHeader::hdr_size(xiah->dnode + xiah->snode);

    if (p->network_length() < hdr_len)
        sa << "truncated-xia";
    else {
		print_xids(sa,xiah);

        sa << ", LAST " << (int)xiah->last;

		if (_print_hlim)
			sa << ", HLIM " << (int)xiah->hlim;
		if (_print_len)
			sa << ", PLEN " << ntohs(xiah->plen);

        if (xiah->nxt == CLICK_XIA_NXT_CID) {
            ContentHeader chdr(p);
            if (chdr.opcode() == ContentHeader::OP_RESPONSE) 
            sa << ", EXT_CONTENT " << "<OP RESPONSE OFF " << chdr.offset() << " CHUNK_OFF " 
               << chdr.chunk_offset() << " LEN " << chdr.length() << " CHUNK_LEN " << chdr.chunk_length() <<"> " ;
            else if (chdr.opcode() == ContentHeader::OP_REQUEST)
            sa << ", EXT_CONTENT " << "<OP REQUEST> "; 
        }

		// print payload
		if (_contents > 0) {
			// TODO: print payload
		}
    }

    sa << '\n';

#if CLICK_USERLEVEL
    if (_outfile) {
		sa << '\n';
		ignore_result(fwrite(sa.data(), 1, sa.length(), _outfile));
    } else
#endif
	_errh->message("%s", sa.c_str());

    return p;
}

void
XIAPrint::add_handlers()
{
    add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE | Handler::CHECKBOX | Handler::CALM, &_active);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAPrint)
