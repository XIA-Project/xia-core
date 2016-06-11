// -*- related-file-name: "../include/click/xiatransportheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaextheader.hh>
#include <click/xiatransportheader.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

TransportHeaderEncap::TransportHeaderEncap(char type)
{
	_hdr->type = type;
	_tcphdr = NULL;
	_options = NULL;
	_optlen = 0;
}

void
TransportHeaderEncap::update()
{
	if (_hdr->type != SOCK_STREAM)
		return;

	size_t padding = 0;
    size_t size = sizeof(struct click_xia_ext);

	size += sizeof(struct click_tcp);
	size += _optlen;

	if ((size & 3) != 0) {
        padding = 4 - (size & 3);
        size += padding;
    }

	u_char *p = new uint8_t[size];
    click_xia_ext* new_hdr = reinterpret_cast<struct click_xia_ext*>(p);

	struct click_tcp *t = reinterpret_cast<struct click_tcp*>(p + sizeof(struct click_xia_ext));
	u_char *o = p + sizeof(struct click_xia_ext) + sizeof(struct click_tcp);

    memcpy(new_hdr, _hdr, sizeof(struct click_xia_ext));

	if (_tcphdr) {
		memcpy(t, _tcphdr, sizeof(struct click_tcp));
	}

	if (_optlen) {
		memcpy(o, _options, _optlen);
	}

	if (padding) {
		memset(o + _optlen, 0, padding);
	}

    new_hdr->hlen = size;

    delete [] reinterpret_cast<uint8_t*>(_hdr);
    _hdr = new_hdr;
}

CLICK_ENDDECLS
