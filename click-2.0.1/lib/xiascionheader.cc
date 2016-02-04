// -*- related-file-name: "../include/click/xiaextheader.hh" -*-
#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaextheader.hh>
#include <click/xiascionheader.hh>
CLICK_DECLS

ScionHeaderEncap::ScionHeaderEncap(const uint8_t *opaque_field, uint8_t length)
{
	this->map()[ScionHeader::OPAQUE_FIELD]= String((const char*)&opaque_field, length);
	this->map()[ScionHeader::LENGTH]= String((const char*)&length, sizeof(length));
    this->update();
}

CLICK_ENDDECLS
